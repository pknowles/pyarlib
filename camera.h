/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#pragma once
/* author: Pyar Knowles, date: 14/05/10 */

#ifndef CAMERA_H
#define CAMERA_H

#define CAMERA_DEFAULT_FOV (75 * pi / 180.0f)
#define CAMERA_DEFAULT_SENSOR_HEIGHT 24.0f

#include "vec.h"
#include "matrix.h"
#include "quaternion.h"

#define CAMERA_STORE_FOCAL_LENGTH 0

class Camera
{
private:
	//projection matrices
	bool bPerspective;
	float aspect;
	bool bDirtyRotation;
	
	//camera vars
	float orbitDistance;
	vec3f position;
	bool bEuler;
	vec3f euler; //primary rotation in angles
	Quat quaternion; //primary rotation as quaternion
	
	bool hasOffset;
	Quat rotOffset; //secondary rotation

	//cached camera vars, derrived from above
	mat44 transform;
	mat44 inverse;
	Quat rotation;
	
	//projection depth vars
	mat44 projection;
	mat44 projectionInv;
	float nearPlane;
	float farPlane;
	
	//projection width/height vars
	float size;
	float fov;
	
	void updateRotation();
public:
	Camera();
	void lock(bool gimbalLock);
	void regen();
	void regenProjection();
	void regenCamera();
	void setZoom(float d);
	void lookAt(vec3f from, vec3f to);
	void zoomAt(vec3f from, vec3f to);
	void copyProjection(const Camera& other);
	void setAspectRatio(float r);
	void setDistance(float n, float f);
	void setClipPlanes(float nearPlane, float farPlane);
	void setClipPlanes(vec2f nearFar);
	void setPerspective(float a);
	void setOrthographic(float s);
	void setPosition(const vec3f& pos);
	void setRotation(const vec2f& angle);
	void setRotation(const vec3f& angle);
	void setRotation(const Quat& rotation);
	void setOffset(const Quat& rotation);
	void setOffset(const vec3f& angle);
	void setFOV(float a); //vertical field of view. cos graphics just has to be different. or because screens are landscape
	void projectMouse(const vec2f& normalizedMouse, vec3f& start, vec3f& end);
	void move(const vec3f& dir);
	void rotate(const vec2f& angle);
	void rotate(const vec3f& angle);
	void rotate(const Quat& delta);
	void zoom(float d);
	void scale(float s); //scales position, zoom and near/far planes
	float getAspectRatio() const;
	const vec3f& getPosition() const;
	const Quat& getOffset() const;
	const Quat getRotation();
	const vec3f getEuler();
	float getFOVY() const;
	float getFOVX() const; //taking into account aspect ratio
	bool isOrthographic() const {return !bPerspective;}
	bool isPerspective() const {return bPerspective;}
	vec2f getNearSize() const;
	vec3f getZoomPos();
	vec2f getClipPlanes() const;
	const mat44& getTransform() const;
	const mat44& getInverse() const;
	const mat44& getProjection() const;
	const mat44& getProjectionInv() const;
	vec3f toVec();
	vec3f upVec();
	vec3f rightVec();
	float getZoom() const;
	void load(std::string filename);
	void save(std::string filename);
	
	static Camera identity();
	
	//used for depth of field with thin lens approximation. just helper/storage, doesn't affect rest of Camera class
	//note: fov remains constant. thus, true f-number, focal length or sensor height may change while focusing
	//1. when storing focal length, camera focuses by changing the sensor size and position
	//2. when storing sensor height, camera focuses by changing the focal length and keeping the sensor still
	float aperture; //radius of the aperture
	float focus; //distance from camera to plane of stuff that should be in focus
	#if CAMERA_STORE_FOCAL_LENGTH
	float focalLength;
	#else
	float sensorHeight;
	#endif
	void moveFocus(float newFocus, float step); //changes focus non-linearly by interpolating either focal length or sensor position/size
	void setBlurryness(float B); //a thing I made up, to avoid providing two parameters. B = aperture / sensorSize
	void setFNumber(float N); //changes aperture to match an f-number (set by FOV and either sensor size or focal length constraints)
	float getFNumber() const;
	float getSillyFNumber() const; //f-number if focused at infinity
	void setFocalLength(float f);
	float getFocalLength() const;
	float getSillyFocalLength() const; //focal length at infinity
	void setSensorHeight(float h);
	float getSensorHeight() const;
	void setSensorDistance(float d); //adjusts focus to match the given sensor distance
	float getSensorDistance() const; //given aperture and sensor size, this returns the distance at which the sensor must exist to provide the correct FOV
	float getInfinityCoC() const; //normalized to ratio of sensor size
	float getMagnification() const;
	float getFocusPlaneHeight() const;
};

std::ostream& operator<<(std::ostream &out, Camera &c);
std::istream& operator>>(std::istream &in, Camera &c);

#endif
