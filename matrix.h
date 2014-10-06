/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#pragma once
#ifndef MATRIX_H
#define MATRIX_H

#include "vec.h"

#define PRINTMAT33(mat) printf("mat33('%s'):\n[ %+.2f %+.2f %+.2f ]\n[ %+.2f %+.2f %+.2f ]\n[ %+.2f %+.2f %+.2f ]\n", \
	#mat, \
	mat.m[0],mat.m[3],mat.m[6], \
	mat.m[1],mat.m[4],mat.m[7], \
	mat.m[2],mat.m[5],mat.m[8])
#define PRINTMAT44(mat) printf("mat44('%s'):\n[ %+.4f %+.4f %+.4f %+.4f ]\n[ %+.4f %+.4f %+.4f %+.4f ]\n[ %+.4f %+.4f %+.4f %+.4f ]\n[ %+.4f %+.4f %+.4f %+.4f ]\n", \
	#mat, \
	mat.m[0],mat.m[4],mat.m[8],mat.m[12], \
	mat.m[1],mat.m[5],mat.m[9],mat.m[13], \
	mat.m[2],mat.m[6],mat.m[10],mat.m[14], \
	mat.m[3],mat.m[7],mat.m[11],mat.m[15])

struct mat44;

struct mat33
{
	struct Column
	{
		float r1, r2, r3;
		Column& operator=(const vec3f& v);
	};
	union
	{
		float m[9];
		float d[3][3];
		struct
		{
			Column c1, c2, c3;
		};
	};
	mat33();
	mat33(const mat44& o);
	mat33 transpose() const;
};

struct mat44
{
	struct Column
	{
		float r1, r2, r3, r4;
		Column& operator=(const vec4f& v);
	};
	union
	{
		float m[16];
		float d[4][4];
		struct
		{
			Column c1, c2, c3, c4;
		};
	};
	operator float*() {return m;}
	mat44();
	mat44(const mat44& o);
	mat44(const float o[16]);
	mat44(const float o[4][4]);
	mat44(float m11, float m12, float m13, float m14, //FIXME: NOT CONSISTENT ROW/COLUMN MAJOR
		float m21, float m22, float m23, float m24,
		float m31, float m32, float m33, float m34,
		float m41, float m42, float m43, float m44);
	mat44(const vec3f a, const vec3f b, const vec3f c, const vec3f d = vec3f(0.0f));
	mat44(const vec4f a, const vec4f b, const vec4f c, const vec4f d = vec4f(0.0f));
	mat44& operator=(const mat44& o);
	float& operator[](int i);
	float& operator()(int i, int j);
	vec3f operator*(const vec3f& v) const;
	vec4f operator*(const vec4f& v) const;
	mat44 operator*(float d) const;
	void operator*=(float d);
	mat44 operator*(const mat44& o) const;
	void operator*=(const mat44& o);
	mat44 operator+(const mat44& o) const;
	void operator+=(const mat44& o);
	mat44 operator-(const mat44& o) const;
	void operator-=(const mat44& o);
	float cofactor(int i, int j) const;
	float det() const;
	mat44 transpose() const;
	mat44 inverse() const;
	void print();

	static mat44 zero();
	static mat44 filled(float v);
	static mat44 identity();
	static mat44 identity(float v);
	static mat44 rotate(float yaw, float pitch, float roll);
	static mat44 rotate(const vec3f& euler);
	static mat44 rotate(float a, vec3f v);
	static mat44 translate(float x, float y, float z);
	static mat44 translate(const vec3f& pos);
	static mat44 scale(float s);
	static mat44 scale(float x, float y, float z);
	static mat44 scale(const vec3f& pos);
};

vec4f operator*(const vec4f& v, const mat44& m);
vec4f& operator*=(vec4f& v, const mat44& m);

#endif
