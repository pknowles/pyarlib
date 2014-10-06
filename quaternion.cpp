/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <stdio.h>

#include "quaternion.h"

const float quatTolerance = 0.00001f;

const float pi = 3.14159265f;

Quat::Quat()
{
}
Quat::Quat(float nx, float ny, float nz, float nw)
{
	x = nx;
	y = ny;
	z = nz;
	w = nw;
}
Quat::Quat(float angle, vec3f vec)
{
	vec.normalize();
	angle *= 0.5f;
	float sina = sinf(angle);
	w = cosf(angle);
	x = vec.x * sina;
	y = vec.y * sina;
	z = vec.z * sina;
}
Quat::Quat(mat33 m)
{
	//http://www.flipcode.com/documents/matrfaq.html#Q55
	// 0  3  6
	// 1  4  7
	// 2  5  8
	float T = m.m[0] + m.m[4] + m.m[8] + 1.0f;
	float S;
	if (T > 0.0f)
	{
		S = 0.5f / sqrt(T);
		x = (m.m[5] - m.m[7]) * S;
		y = (m.m[6] - m.m[2]) * S;
		z = (m.m[1] - m.m[3]) * S;
		w = 0.25f / S;
	}
	else
	{
		//a/b/c = array indices greater than others. find maximum index
		//a = 0 > 1
		//b = 0 > 2
		//c = 1 > 2
		
		// +true/!false: order = decimal = binary
		//!a !b !c: 0 1 2 = 2 = 1 0
		//!a !b +c: 0 2 1 = 1 = 0 1
		//+a !b !c: 1 0 2 = 2 = 1 0
		//+a +b !c: 1 2 0 = 0 = 0 0
		//!a +b +c: 2 0 1 = 1 = 0 1
		//+a +b +c: 2 1 0 = 0 = 0 0
		
		//reorder for binary repr
		//!a !b !c: 0 1 2 = 2 = 1 0
		//!a !b +c: 0 2 1 = 1 = 0 1
		//!a +b !c: not possible
		//!a +b +c: 2 0 1 = 1 = 0 1
		//+a !b !c: 1 0 2 = 2 = 1 0
		//+a !b +c: not possible
		//+a +b !c: 1 2 0 = 0 = 0 0
		//+a +b +c: 2 1 0 = 0 = 0 0
		
		//bit 1 = !bc + !ab (from: http://www.ee.calpoly.edu/media/uploads/resources/KarnaughExplorer_1.html)
		//bit 2 = !a!c + a!b
		
		static int precompModulo[3] = {1, 2, 0};
		int i = 0;
		if (m.m[4] > m.m[0])
			i = 1;
		if (m.m[8] > m.d[i][i])
			i = 2;
		int j = precompModulo[i+1];
		int k = precompModulo[i+2];
		
		S = sqrt(m.d[i][i] - m.d[j][j] - m.d[k][k] + 1.0f);
		(*this)[i] = 0.5f / S;
		S = 0.5f / S;
		w = (m.d[k][j] - m.d[j][k]) * S;
		(*this)[j] = (m.d[j][i] + m.d[i][j]) * S;
		(*this)[k] = (m.d[k][i] + m.d[i][k]) * S;
		printf("FIXME: using untested code\n");
	}
}
Quat::Quat(mat44 m)
{
	//http://www.flipcode.com/documents/matrfaq.html#Q55
	// 0  4  8  12
	// 1  5  9  13
	// 2  6  10 14
	// 3  7  11 15
	float S;
	float T = m.m[0] + m.m[5] + m.m[10] + 1.0f;
	if (T > 0.0f)
	{
		S = 0.5f / sqrt(T);
		x = (m.m[6] - m.m[9]) * S;
		y = (m.m[8] - m.m[2]) * S;
		z = (m.m[1] - m.m[4]) * S;
		w = 0.25f / S;
	}
	else
	{
		static int precompModulo[3] = {1, 2, 0};
		int i = 0;
		if (m.m[5] > m.m[0])
			i = 1;
		if (m.m[10] > m.d[i][i])
			i = 2;
		int j = precompModulo[i+1];
		int k = precompModulo[i+2];
		
		S = sqrt(m.d[i][i] - m.d[j][j] - m.d[k][k] + 1.0f) * 2.0f;
		(*this)[i]= 0.5f / S;
		w = (m.d[k][j] - m.d[j][k]) / S;
		(*this)[j] = (m.d[j][i] + m.d[i][j]) / S;
		(*this)[k] = (m.d[k][i] + m.d[i][k]) / S;
		printf("FIXME: using untested code\n");
	}
}
Quat Quat::operator+(const Quat& A) const
{
	return Quat(x+A.x, y+A.y, z+A.z, w+A.w);
}
Quat Quat::operator-(const Quat& A) const
{
	return Quat(x-A.x, y-A.y, z-A.z, w-A.w);
}
Quat Quat::operator*(float d) const
{
	return Quat(x*d, y*d, z*d, w*d);
}
Quat Quat::operator*(const Quat& A) const
{
	return Quat(
		x*A.w + w*A.x + y*A.z - z*A.y,
		w*A.y - x*A.z + y*A.w + z*A.x,
		w*A.z + x*A.y - y*A.x + z*A.w,
		w*A.w - x*A.x - y*A.y - z*A.z);
}
vec3f Quat::operator*(const vec3f& v) const
{
	float m[9];
	m[0] = w*w+x*x-y*y-z*z; m[1] = 2*x*y-2*w*z;     m[2] = 2*x*z + 2*w*y;
	m[3] = 2*x*y + 2*w*z;   m[4] = w*w-x*x+y*y-z*z; m[5] = 2*y*z - 2*w*x;
	m[6] = 2*x*z - 2*w*y;   m[7] = 2*y*z + 2*w*x;   m[8] = w*w-x*x-y*y+z*z;
	vec3f r;
	r.x = m[0] * v.x + m[1] * v.y + m[2] * v.z;
	r.y = m[3] * v.x + m[4] * v.y + m[5] * v.z;
	r.z = m[6] * v.x + m[7] * v.y + m[8] * v.z;
	return r;
}
Quat Quat::operator/(float d) const
{
	return Quat(x/d, y/d, z/d, w/d);
}
Quat Quat::operator/(const Quat& A) const
{
	float qq =
		A.x * A.x + 
		A.y * A.y + 
		A.z * A.z + 
		A.w * A.w;

	return *this * Quat(
		-A.x/qq,
		-A.y/qq,
		-A.z/qq,
		A.w/qq
		);
}
void Quat::operator+=(const Quat& A)
{
	x += A.x;
	y += A.y;
	z += A.z;
	w += A.w;
}
void Quat::operator-=(const Quat& A)
{
	x -= A.x;
	y -= A.y;
	z -= A.z;
	w -= A.w;
}
void Quat::operator*=(float d)
{
	x *= d;
	y *= d;
	z *= d;
	w *= d;
}
void Quat::operator*=(const Quat& A)
{
	float tx = x*A.w + w*A.x + y*A.z - z*A.y;
	float ty = w*A.y - x*A.z + y*A.w + z*A.x;
	float tz = w*A.z + x*A.y - y*A.x + z*A.w;
	w = w*A.w - x*A.x - y*A.y - z*A.z;
	x = tx;
	y = ty;
	z = tz;
}
void Quat::operator/=(float d)
{
	x /= d;
	y /= d;
	z /= d;
	w /= d;
}
/*
float Quat::operator[](int index)
{
	return ((float*)this)[index];
}
*/
Quat::operator Array4()
{
	return l;
}
bool Quat::operator==(const Quat& A)
{
	return x == A.x &&
		y == A.y &&
		z == A.z &&
		w == A.w;
}
bool Quat::operator!=(const Quat& A)
{
	return x != A.x ||
		y != A.y ||
		z != A.z ||
		w != A.w;
}
void Quat::angleAxis(float& a, vec3f& v) const
{
	float wsign = w>0.0f?1.0f:-1.0f; //get shortest direction
	float ww = sqrt(1.0f - w*w) * wsign;
	if (ww == 0.0f)
		v = vec3f(0, 1, 0);
	else
		v = vec3f(x / ww, y / ww, z / ww);
	a = 2.0f * acos(w*wsign);
}
float Quat::getAngle(void) const
{
	return 2.0f * acos(fabs(w));
}
vec3f Quat::getAxis(void) const
{
	float ww = sqrt(1.0f - w*w);
	if (ww == 0.0f)
		return vec3f(0, 1, 0);
	else
		return vec3f(x / ww, y / ww, z / ww);
}
Quat Quat::inverse(void) const
{
	float qq =
		x * x + 
		y * y + 
		z * z + 
		w * w;
	return Quat(-x, -y, -z, w) / qq;
}
float Quat::dot(const Quat& A) const
{
	return
		x * A.x +
		y * A.y +
		z * A.z +
		w * A.w;
}
vec3f Quat::euler(void) const
{
	//awwwwesome: http://www.3dgametechnology.com/wp/converting-quaternion-to-euler-angle/
	float sqx = x * x;
	float sqy = y * y;
	float sqz = z * z;
	float sqw = w * w;

	float unit = sqx + sqy + sqz + sqw;
	float test = (x * w - y * z);
	
	if (test > 0.4999999f * unit)
	{
		return vec3f(pi / 2.0f, 2.0f*atan2(y, w), 0.0f);
	}
	else if (test < -0.4999999f * unit)
	{
		return vec3f(-pi / 2.0f, 2.0f*atan2(y, w), 0.0f);
	}
    else
    {
		return vec3f(
			asin(2.0f*(x*w-y*z)),
			atan2(2.0f*(x*z+y*w), 1.0f - 2.0f*(sqx+sqy)),
			atan2(2.0f*(x*y+z*w), 1.0f - 2.0f*(sqx+sqz))
			);
	}
	
	//stupid fail coordinate systems

	//nope: http://forums.create.msdn.com/forums/t/4574.aspx
	//atan2(2.0f*(x*w-y*z), 1.0f - 2.0f*(x*x+z*z)),
	//atan2(2.0f*(y*w-x*z), 1.0f - 2.0f*(y*y+z*z)),
	//asin(2.0f*(x*y+z*w))
	
	//full of crap: http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/
	//atan2(2.0f*(y*w-x*z), 1.0f - 2.0f*(y*y+z*z)),
	//asin(2.0f*(x*y + z*w)),
	//atan2(2.0f*(x*w-y*z), 1.0f - 2.0f*(x*x+z*z))
		
	//full of crap: http://en.wikipedia.org/wiki/Euler_angles
	//atan2(2.0f*(w*x+y*z), 1.0f - 2.0f*(x*x+y*y)),
	//asin(2.0f*(w*y - z*x)),
	//atan2(2.0f*(w*z+x*y), 1.0f - 2.0f*(y*y+z*z))
}
float Quat::sqsize(void) const
{
	return x * x + 
		y * y + 
		z * z + 
		w * w;
}
float Quat::size(void) const
{
	return sqrt(x * x + 
		y * y + 
		z * z + 
		w * w);
}
Quat Quat::unit(void) const
{
	float size = sqrt(
		x * x + 
		y * y + 
		z * z + 
		w * w
		);
	return Quat(x/size, y/size, z/size, w/size);
}
void Quat::normalize(void)
{
	float size = sqrt(
		x * x + 
		y * y + 
		z * z + 
		w * w
		);
	if (w < 0)
		size = -size;
	x /= size;
	y /= size;
	z /= size;
	w /= size;
}
mat44 Quat::getMatrix()
{
	//from: http://gpwiki.org/index.php/OpenGL:Tutorials:Using_Quaternions_to_represent_rotation
	float x2 = x * x;
	float y2 = y * y;
	float z2 = z * z;
	float xy = x * y;
	float xz = x * z;
	float yz = y * z;
	float wx = w * x;
	float wy = w * y;
	float wz = w * z;
	return mat44( 1.0f - 2.0f * (y2 + z2), 2.0f * (xy - wz), 2.0f * (xz + wy), 0.0f,
		2.0f * (xy + wz), 1.0f - 2.0f * (x2 + z2), 2.0f * (yz - wx), 0.0f,
		2.0f * (xz - wy), 2.0f * (yz + wx), 1.0f - 2.0f * (x2 + y2), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}
Quat Quat::slerp(Quat q, float d) const
{
	float cosHalfTheta = dot(q);
	if (fabs(cosHalfTheta) >= 1.0f)
		return *this;
	if (cosHalfTheta < 0.0)
	{
		q = -q;
		cosHalfTheta = -cosHalfTheta;
	}
	float halfTheta = acos(cosHalfTheta);
	float sinHalfTheta = sqrt(1.0f - cosHalfTheta * cosHalfTheta);
	if (fabs(sinHalfTheta) < 0.0001)
		return (*this + q) * 0.5;
	float a = sin((1.0f - d) * halfTheta) / sinHalfTheta;
	float b = sin(d * halfTheta) / sinHalfTheta;
	return (*this * a + q * b).unit(); //wtf shouldn't have to normalize
}
Quat Quat::lerp(Quat q, float d) const
{
	return *this + (q - *this) * d;
}
Quat Quat::nlerp(Quat q, float d) const
{
	return *this; //NOT IMPLEMENTED
}
Quat Quat::identity()
{
	Quat r;
	r.x = 0.0f;
	r.y = 0.0f;
	r.z = 0.0f;
	r.w = 1.0f;
	return r;
}
Quat operator-(const Quat& q) //negate
{
	Quat r;
	r.w = -q.w; //FIXME: should w be kept positive?
	r.x = -q.x;
	r.y = -q.y;
	r.z = -q.z;
	return r;
}
Quat Quat::fromEuler(float yaw, float pitch, float roll)
{
	Quat r;

	yaw *= 0.5f;
	pitch *= 0.5f;
	roll *= 0.5f;
	float c1 = cos(yaw);
	float s1 = sin(yaw);
	float c2 = cos(pitch);
	float s2 = sin(pitch);
	float c3 = cos(roll);
	float s3 = sin(roll);
	
	r.w = c1*c2*c3 + s1*s2*s3;
	r.x = s1*c2*c3 - c1*s2*s3;
	r.y = c1*s2*c3 + s1*c2*s3;
	r.z = c1*c2*s3 - s1*s2*c3;
	return r;
}
Quat Quat::fromEuler(const vec2f& angles)
{
	return fromEuler(angles.x, angles.y, 0.0f);
}
Quat Quat::fromEuler(const vec3f& angles)
{
	return fromEuler(angles.x, angles.y, angles.z);
}
Quat Quat::fromTo(const vec3f& from, const vec3f& to)
{
	float cosAngle = from.dot(to);
	vec4f axis(
		from.cross(to),
		sqrt(from.sizesq()*to.sizesq()) + cosAngle
		);
	
	if (axis.w < 0.000001f)
	{
		int i = 0;
		vec3f mag = vmax(from, -from);
		if (mag.y < mag.x)
			i = 1;
		if (mag.z < mag[i])
			i = 2;
		axis[i] = 1.0f;
	}
	return Quat(axis.x, axis.y, axis.z, axis.w).unit();
	
#if 0
	//from http://www.euclideanspace.com/maths/algebra/vectors/angleBetween/index.htm
	Quat r;
	float d = from.dot(to);
	vec3f axis = from.cross(to);
	float qw = sqrt(from.sizesq()*to.sizesq()) + d;

	//r.w = qw; r.x = axis.x; r.y = axis.y; r.z = axis.z;
	//r.w = r.w < 0.0f ? -r.w : r.w;
	
	//doesn't work according to jesse
	if (qw < 0.000001f)
	{
		//vectors are 180 degrees apart
		r.w = 0.0f; r.x = -from.z; r.y = from.y; r.z = from.x;
	}
	else
	{
		r.w = qw; r.x = axis.x; r.y = axis.y; r.z = axis.z;
	}
	
	if (d == 0.0f)
		r.x = 1.0f;
	
	r.normalize();
	return r;
#endif
}
Quat Quat::dirUp(const vec3f& dir, const vec3f& up)
{
	vec3f forward = dir.unit();
	vec3f upward = up.unit();
	vec3f right = forward.cross(upward).unit();
	upward = right.cross(forward).unit(); //FIXME: may be zero. do I need to check here?
	forward = -forward; //camera looks into -Z
	
	//assert(fabs(forward.dot(upward)) < 0.001);
	//assert(fabs(upward.dot(right)) < 0.001);
	//assert(fabs(right.dot(forward)) < 0.001);
	
	mat33 m;
	m.c1 = right;
	m.c2 = upward;
	m.c3 = forward;
	
	//PRINTMAT33(m);
	
	return Quat(m);
}
Quat Quat::random()
{
	//http://jmonkeyengine.org/forum/topic/random-rotation/
	float u1 = rand()/(float)RAND_MAX;
	float u2 = rand()/(float)RAND_MAX;
	float u3 = rand()/(float)RAND_MAX;
	float u1sqrt = sqrt(u1);
	float u1m1sqrt = sqrt(1.0f - u1);
	float x = u1m1sqrt * sin(2*pi*u2);
	float y = u1m1sqrt * cos(2*pi*u2);
	float z = u1sqrt * sin(2*pi*u3);
	float w = u1sqrt * cos(2*pi*u3);
	return Quat(x, y, w, z);
}



