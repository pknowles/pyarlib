/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"
/* author: Pyar Knowles, date: 14/05/10 */

#include "camera.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>

Camera::Camera()
{
	bPerspective = false;
	hasOffset = false;
	bEuler = true;
	bDirtyRotation = false;
	transform = inverse = projection = mat44::zero();
	quaternion = Quat::identity();
	rotOffset = Quat::identity();
	rotation = Quat::identity();
	position(0.0f);
	euler(0.0f);
	orbitDistance = 0.0f;
	setDistance(0.1f, 100.0f);
	setPerspective(CAMERA_DEFAULT_FOV);
	setAspectRatio(1.0f);
	regen();
	
	#if CAMERA_STORE_FOCAL_LENGTH
	focalLength = 0.030f;
	#else
	sensorHeight = CAMERA_DEFAULT_SENSOR_HEIGHT/1000.0f;
	#endif
	
	focus = 999.0f;
	//aperture = sensorHeight;
	setFNumber(2.8f);
}
void Camera::updateRotation()
{
	if (!bDirtyRotation)
		return;

	if (bEuler)
	{
		euler.x = fmod(euler.x, 2.0f * pi);
		euler.y = fmod(euler.y, 2.0f * pi);
		euler.z = fmod(euler.z, 2.0f * pi);
		if (euler.x >= pi*0.5f) euler.x = pi*0.5f-0.001f;
		if (euler.x <= -pi*0.5f) euler.x = -pi*0.5f+0.001f;
		if (euler.y > pi) euler.y -= pi*2.0f;
		if (euler.y <= -pi) euler.y += pi*2.0f;
		if (euler.z > pi) euler.y -= pi*2.0f;
		if (euler.z <= -pi) euler.y += pi*2.0f;
		quaternion = Quat::fromEuler(euler);
	}
	
	if (hasOffset)
		rotation = rotOffset * quaternion;
	else
		rotation = quaternion;
}
void Camera::lock(bool gimbalLock)
{
	if (gimbalLock == bEuler)
		return;
	if (gimbalLock)
		euler = quaternion.euler();
	else
		updateRotation();
	bEuler = gimbalLock;
	printf("Camera in %s mode\n", bEuler?"euler":"quaternion");
}
void Camera::regen()
{
	regenProjection();
	regenCamera();
}
void Camera::regenProjection()
{
	if (bPerspective)
	{
		float tanfov = tan(fov*0.5f);
		float t = nearPlane * tanfov;
		float r = nearPlane * tanfov * aspect;
		projection.m[0*4+0] = nearPlane / r;
		projection.m[1*4+1] = nearPlane / t;
		projection.m[2*4+2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
		projection.m[3*4+2] = -2.0f * (farPlane * nearPlane) / (farPlane - nearPlane);
		projection.m[2*4+3] = -1.0f;
	}
	else
	{
		float t = size * 0.5f;
		float r = size * 0.5f * aspect;
		projection.m[0*4+0] = 1.0f / r;
		projection.m[1*4+1] = 1.0f / t;
		projection.m[2*4+2] = -2.0f / (farPlane - nearPlane);
		projection.m[3*4+2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
		projection.m[3*4+3] = 1.0f;
	}
	projectionInv = projection.inverse();
}
void Camera::regenCamera()
{
	updateRotation();
	transform = mat44::translate(position);
	transform *= rotation.getMatrix();
	
	/*
	if (bEuler)
	{
		transform *= mat44::rotate(euler.y, vec3f(0.0, 1.0, 0.0));
		transform *= mat44::rotate(euler.x, vec3f(1.0, 0.0, 0.0));
		transform *= mat44::rotate(euler.z, vec3f(0.0, 0.0, 1.0));
	}
	*/
	
	if (orbitDistance > 0.0f)
		transform *= mat44::translate(0.0, 0.0, orbitDistance);
	/*
	if (false)
	{
		transform *= mat44::rotate(bump.z, vec3f(0.0, 0.0, 1.0));
		transform *= mat44::rotate(bump.y, vec3f(0.0, 1.0, 0.0));
		transform *= mat44::rotate(bump.x, vec3f(1.0, 0.0, 0.0));
	}
	*/
	/*
	transform *= mat44::rotate(euler.x, vec3f(1.0, 0.0, 0.0));
	transform *= mat44::rotate(euler.x, vec3f(0.0, 1.0, 0.0));
	transform *= mat44::rotate(euler.x, vec3f(0.0, 0.0, 1.0));
	*/
	inverse = transform.inverse();
}
void Camera::setZoom(float d)
{
	orbitDistance = d;
}
void Camera::lookAt(vec3f from, vec3f to)
{
	vec3f dir = to - from;
	if (dir.sizesq() == 0.0f) return;
	setRotation(vec2f::fromVec(dir));
	setPosition(from);
	orbitDistance = 0.0f;
}
void Camera::zoomAt(vec3f from, vec3f to)
{
	vec3f dir = to - from;
	float lensq = dir.sizesq();
	if (lensq == 0.0f) return;
	setRotation(vec2f::fromVec(dir));
	setPosition(to);
	setZoom(sqrt(lensq));
}
void Camera::copyProjection(const Camera& other)
{
	if (bPerspective)
		setFOV(other.getFOVY());
	else
		size = other.size; //FIXME: should probably have more values than just size, such as front/back
	setDistance(other.getClipPlanes().x, getClipPlanes().y);
	setAspectRatio(other.getAspectRatio());
	regenProjection(); //just in case other has dirty projection
}
void Camera::setAspectRatio(float r)
{
	aspect = r;
}
void Camera::setDistance(float n, float f)
{
	setClipPlanes(n, f);
}
void Camera::setClipPlanes(float nearPlane, float farPlane)
{
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
}
void Camera::setClipPlanes(vec2f nearFar)
{
	setClipPlanes(nearFar.x, nearFar.y);
}
void Camera::setPerspective(float a)
{
	if (a > pi)
		printf("Error: Camera perspective greater than pi.\n");
	if (!bPerspective)
		projection = mat44::zero();
	bPerspective = true;
	fov = a;
}
void Camera::setOrthographic(float s)
{
	if (bPerspective)
		projection = mat44::zero();
	bPerspective = false;
	size = s;
}
void Camera::setPosition(const vec3f& pos)
{
	position = pos;
}
void Camera::setRotation(const vec3f& angle)
{
	if (bEuler)
		euler = angle;
	else
		quaternion = Quat::fromEuler(angle);
	bDirtyRotation = true;
}
void Camera::setRotation(const vec2f& angle)
{
	euler = vec3f(angle, 0.0f);
	bDirtyRotation = true;
}
void Camera::setRotation(const Quat& rotation)
{
	if (bEuler)
		euler = rotation.euler();
	else
		quaternion = rotation;
	bDirtyRotation = true;
}
void Camera::setOffset(const Quat& rotation)
{
	rotOffset = rotation;
	hasOffset = !(rotOffset.x == 0.0f && rotOffset.y == 0.0f && rotOffset.z == 0.0f && rotOffset.w == 1.0f);
	bDirtyRotation = true;
}
void Camera::setFOV(float a)
{
	if (a > pi)
		printf("Error: Camera perspective greater than pi.\n");
	if (!bPerspective)
		printf("Warning: Setting FOV for non-perspective camera\n");
	fov = a;
}
void Camera::projectMouse(const vec2f& normalizedMouse, vec3f& start, vec3f& end)
{
	vec4f a, b;
	a = transform * (projectionInv * vec4f(normalizedMouse*2.0f-1.0f, -1.0f, 1.0f));
	b = transform * (projectionInv * vec4f(normalizedMouse*2.0f-1.0f, 1.0f, 1.0f));
	start = a.xyz() / a.w;
	end = b.xyz() / b.w;
}
void Camera::move(const vec3f& dir)
{
	position += dir;
}
void Camera::rotate(const vec3f& angle)
{
	if (bEuler)
		euler += angle;
	else
		quaternion *= Quat::fromEuler(angle);
	bDirtyRotation = true;
}
void Camera::rotate(const vec2f& angle)
{
	if (bEuler)
		euler += angle;
	else
		quaternion *= Quat::fromEuler(vec3f(angle, 0.0f));
	bDirtyRotation = true;
}
void Camera::rotate(const Quat& delta)
{
	if (bEuler)
		euler += delta.euler();
	else
		quaternion *= delta;
	bDirtyRotation = true;
}
void Camera::zoom(float d)
{
	orbitDistance += d;
	if (orbitDistance < 0.0f)
		orbitDistance = 0.0f;
}
void Camera::scale(float s)
{
	nearPlane *= s;
	farPlane *= s;
	position *= s;
	orbitDistance *= s;
}
float Camera::getAspectRatio() const
{
	return aspect;
}
const vec3f& Camera::getPosition() const
{
	return position;
}
const Quat& Camera::getOffset() const
{
	return rotOffset;
}
const Quat Camera::getRotation()
{
	updateRotation();
	if (bEuler)
		return Quat::fromEuler(euler);
	return quaternion;
}
const vec3f Camera::getEuler()
{
	updateRotation();
	if (bEuler)
		return euler;
	return quaternion.euler();
}
float Camera::getFOVY() const
{
	return fov;
}
float Camera::getFOVX() const
{
	return atan(tan(fov*0.5f) * aspect)*2.0f;
}
vec2f Camera::getNearSize() const
{
	if (bPerspective)
	{
		float tanfov = 2.0f*tan(fov*0.5f);
		return vec2f(nearPlane * tanfov * aspect, nearPlane * tanfov);
	}
	else
		return vec2f(size * aspect, size);
}
vec3f Camera::getZoomPos()
{
	return getPosition() - toVec() * orbitDistance;
}
vec2f Camera::getClipPlanes() const
{
	return vec2f(nearPlane, farPlane);
}
const mat44& Camera::getTransform() const
{
	return transform;
}
const mat44& Camera::getInverse() const
{
	return inverse;
}
const mat44& Camera::getProjection() const
{
	return projection;
}
const mat44& Camera::getProjectionInv() const
{
	return projectionInv;
}
vec3f Camera::toVec()
{
	updateRotation();
	if (bEuler && !hasOffset)
		return vec2f(euler).toVec();
	else
		return rotation * vec3f(0.0f, 0.0f, -1.0f);
}
vec3f Camera::upVec()
{
	updateRotation();
	if (bEuler && !hasOffset)
		return (vec2f(euler) + vec2f(pi*0.5f, 0.0f)).toVec();
	else
		return rotation * vec3f(0.0f, 1.0f, 0.0f);
}
vec3f Camera::rightVec()
{
	updateRotation();
	if (bEuler && !hasOffset)
		return vec2f(0.0f, euler.y - pi*0.5f).toVec();
	else
		return rotation * vec3f(1.0f, 0.0f, 0.0f);
}
float Camera::getZoom() const
{
	return orbitDistance;
}
void Camera::load(std::string filename)
{
	std::ifstream ifile(filename.c_str());
	if (ifile.is_open())
	{
		ifile >> *this;
		ifile.close();
	}
}
void Camera::save(std::string filename)
{
	std::ofstream ofile(filename.c_str());
	ofile << *this;
	ofile.close();
}

void Camera::moveFocus(float newFocus, float step)
{
	#if CAMERA_STORE_FOCAL_LENGTH
	float f = getFocalLength();
	float A = focus * f / (focus - f);
	float B = newFocus * f / (newFocus - f);
	step = mymin(myabs(B - A), step);
	float C = A + mysign(B - A) * step;
	focus = C * f / (C - f);
	#else
	float d = getSensorDistance();
	float A = d * focus / (d + focus);
	float B = d * newFocus / (d + newFocus);
	step = mymin(myabs(B - A), step);
	float C = A + mysign(B - A) * step;
	focus = d * C / (d - C);
	#endif
}

Camera Camera::identity()
{
	Camera r;
	r.setOrthographic(2.0f);
	PRINTMAT44(r.getProjection());
	PRINTMAT44(r.getInverse());
	return r;
}

void Camera::setBlurryness(float B)
{
	aperture = getSensorHeight() * B;
}
void Camera::setFNumber(float N)
{
	aperture = getFocalLength() / N;
	//aperture = 0.032f / N;
}
float Camera::getFNumber() const
{
	return getFocalLength() / aperture;
}
float Camera::getSillyFNumber() const
{
	return getSillyFocalLength() / aperture;
}
void Camera::setFocalLength(float f)
{
	#if CAMERA_STORE_FOCAL_LENGTH
	focalLength = f;
	#else
	sensorHeight = 2.0f * f * tan(fov * 0.5f);
	#endif
}
float Camera::getFocalLength() const
{
	#if CAMERA_STORE_FOCAL_LENGTH
	return focalLength;
	#else
	float d = getSensorDistance();
	return d * focus / (d + focus);
	#endif
}
float Camera::getSillyFocalLength() const
{
	#if CAMERA_STORE_FOCAL_LENGTH
	return focalLength;
	#else
	float d = getSensorDistance();
	return d;
	#endif
}
void Camera::setSensorHeight(float h)
{
	#if CAMERA_STORE_FOCAL_LENGTH
	setSensorDistance(h / (2.0f * tan(fov * 0.5f)));
	#else
	sensorHeight = h;
	#endif
}
float Camera::getSensorHeight() const
{
	#if CAMERA_STORE_FOCAL_LENGTH
	return getSensorDistance() * 2.0f * tan(fov * 0.5f);
	#else
	return sensorHeight;
	#endif
}
void Camera::setSensorDistance(float d)
{
	#if CAMERA_STORE_FOCAL_LENGTH
	setFocalLength(d * focus / (d + focus));
	#else
	setSensorHeight(d * (2.0f * tan(fov * 0.5f)));
	#endif
}
float Camera::getSensorDistance() const
{
	#if CAMERA_STORE_FOCAL_LENGTH
	float f = getFocalLength();
	return focus * f / (focus - f);
	#else
	float h = getSensorHeight();
	return h / (2.0f * tan(fov * 0.5f));
	#endif
}
float Camera::getInfinityCoC() const
{
	float h = getSensorHeight();
	return aperture * getMagnification() / h; //div by sensor size to make relative
}
float Camera::getMagnification() const
{
	float d = getSensorDistance();
	return d / focus;
}
float Camera::getFocusPlaneHeight() const
{
	return 2.0f * focus * tan(fov * 0.5f);
}

std::ostream& operator<<(std::ostream &out, Camera &c)
{
	vec3f pos = c.getPosition();
	vec3f rot = c.getEuler();
	float z = c.getZoom();
	out << pos.x << " " << pos.y << " " << pos.z << std::endl;
	out << rot.x << " " << rot.y << " " << rot.z << std::endl;
	out << z << std::endl;
	return out;
}

std::istream& operator>>(std::istream &in, Camera &c)
{
	vec3f pos;
	vec3f rot;
	float z;
	in >> pos.x >> pos.y >> pos.z;
	in >> rot.x >> rot.y >> rot.z;
	in >> z;
	c.setZoom(z);
	c.setRotation(rot);
	c.setPosition(pos);
	c.regenCamera();
	return in;
}

