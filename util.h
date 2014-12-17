/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef PYARLIB_UTIL_H
#define PYARLIB_UTIL_H

#include <algorithm>
#include <sstream>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#include "vec.h"

#define UNIT_RAND (rand()/(float)RAND_MAX)

extern const float pi;

#ifdef _WIN32
#include <windows.h> //phhhhiwiuhwyas
/*
#if __cplusplus <= 199711L //less than c++11 (afaik)
float log2(float x);
double log2(double x);
#endif
*/
#endif

bool isBigEndian();

int ipow(int base, int exp);
int ilog2(int x);
int ilog10(int x);
int ilog2(int64_t x);
int ilog10(int64_t x);
int ceilLog2(int x);
int ceilSqrt(const int& x);
int isqrt(int x);
int countBits(const int& x);
bool isPowerOf2(const int& x);
int nextPowerOf2(int x);
int resizeWithinFactor(int x, int y, float f = 2.0f); //resize y to f y when x < y/f^2 || x > y
int ceil(const int& n, const int& d);
vec2i ceil(const vec2i& n, const vec2i& d);
unsigned int binomial(unsigned int n, unsigned int k);
int fact(int i); //need optimizing: http://www.luschny.de/math/factorial/FastFactorialFunctions.htm
float normalpdf(float x, float sigma); // use x - mean if nonzero
float standardpdf(float x); //faster normalpdf(x, 1)
float standardcdf(float x);
float gaussianKernal(int x, int n);

vec2f randomOnCircle(float r = 1.0f);
vec2f randomInCircle(float r = 1.0f);
vec3f randomOnSphere(float r = 1.0f);
vec3f randomInSphere(float r = 1.0f);

void createTangents(const vec3f& normal, vec3f& u, vec3f& v); //generates arbitrary but orthonormal tangents for a given normalized direction

//generating more than 1000 is slow
void poissonSquare(std::vector<vec2f>& list, float rmin, float width);
void poissonSquare(std::vector<vec2f>& list, int n);
void poissonDisc(std::vector<vec2f>& list, float rmin, float rdisc);
void poissonDisc(std::vector<vec2f>& list, int n);
void poissonHemisphere(std::vector<vec3f>& list, int n); //WARNING: incorrect, but will do for now
void poissonSphere(std::vector<vec3f>& list, int n); //WARNING: sloow. TODO: spatial data structure. not always n points

void reflect(vec3f &out, const vec3f &incidentVec, const vec3f &normal);
bool refract(vec3f &out, const vec3f &incidentVec, const vec3f &normal, float eta);
void fresnel(float& fReflectance, const vec3f &incidentVec, const vec3f& vNormal, float fDensity1, float fDensity2);

bool intersectCylinder(const vec3f& linePos, const vec3f& lineDir, const vec3f& cylPos, const vec3f& cylDir, float cylRadius, float& t1, float& t2);
bool intersectCylinder(const vec3f& linePos, const vec3f& lineDir, const vec3f& cylPos, const vec3f& cylDir, float cylRadius, float height, float& t1, float& t2);
bool intersectSphere(const vec3f& p, const vec3f& d, float r, float& t1, float& t2);
bool closestPointsLines(const vec3f& p1, const vec3f& d1, const vec3f& p2, const vec3f& d2, float& t1, float& t2);

float chordArea(const vec2f& centre, const float r, const vec2f& a, const vec2f& b);
float areaCircleSquare(vec2f circle, float r, vec2f square); //bottom-left of square is origin

std::string binaryToString(unsigned int x, unsigned int n = 8);
std::string humanBytes(int64_t i, bool use_binary_prefix = true);
std::string humanNumber(float x);
std::string humanNumber(int x);
std::string intToString(int i);
std::string intToString(int i, int padding);
std::string intToString(int i, int padding, char character);
std::string floatToString(float f);
std::string floatToString(float f, int precision);
int stringToInt(const std::string& s);
float stringToFloat(const std::string& s);

void mysleep(float seconds);

template <class T> bool contains(const std::vector<T> &vec, const T &value)
{
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template <class A, class B> bool contains(const std::map<A, B> &m, const A &key)
{
    return m.find(key) != m.end();
}

template <class T> bool contains(const std::set<T> &s, const T &value)
{
    return s.find(value) != s.end();
}

template<class T> std::string toString(const T& v)
{
	std::stringstream s;
	s << v;
	return s.str();
}

vec3f hueToRGB(float h);

//this should put an end to the flood of ambiguous overloads
#define CT(n) static_cast<T>(n)
template<class T> inline const T& myclamp(const T& x, const T& a, const T& b) {return (x<a)?a:((x>b)?b:x);}
template<class T> inline const T mysign(const T& x) {return (x > CT(0)) ? CT(1) : ((x<CT(0)) ? CT(-1) : CT(0));}
template<class T> inline const T& mymin(const T& a, const T& b) {return (a<b) ? a : b;}
template<class T> inline const T& mymax(const T& a, const T& b) {return (a>b) ? a : b;}
template<class T> inline const T myabs(T x) {return (x < CT(0)) ? -x : x;}
#undef CT

//"efficient" clear std container
template<class T> inline void mystdclear(T& v) {T* empty = new T(); std::swap(v, *empty); delete empty;}

float myround(float x);
float myfract(float x);
int mymod(int x, int m);
float mymod(float x, float m);

struct MyTimer
{
	timeval lastTime;
	float time(); //return delta between last time() call
};

template<typename T> inline const T& interpNearest(const T& a, const T& b, float x) {return x < 0.5f ? a : b;}
template<typename T> inline const T interpLinear(const T& a, const T& b, float x) {return a * (1.0f - x) + b * x;}
template<typename T> inline const T interpCosine(const T& a, const T& b, float x) {x = 0.5 - 0.5f * cos(x * pi); return a * (1.0f - x) + b * x;}
template<typename T> inline const T interpCubic(const T& a, const T& b, const T& c, const T& d, float x)
{
	return b + 0.5f * x*(c - a + x*(2.0f*a - 5.0*b + 4.0*c - d + x*(3.0*(b - c) + d - a)));
}
vec3f interpHermite(vec3f a, vec3f aDir, vec3f b, vec3f bDir, float x);
vec3f interpHermiteDeriv(vec3f a, vec3f aDir, vec3f b, vec3f bDir, float x);

std::string getStackString();

template<typename T>
struct my_array_deleter
{
   void operator()(T* p) {delete [] p;}
};

template<typename T>
struct my_swapper {
private:
	bool s;
	T a, b;
public:
	my_swapper() {s = false;}
	operator T&() {return s?a:b;}
	T& next() {return s?b:a;}
	void swap() {s = !s;}
};

std::string format(const std::string fmt, ...);

namespace pyarlib
{
	template<typename T>
	struct on_demand {
	private:
		T* object;
		void release() {delete object; object = NULL;}
	public:
		on_demand() {object = NULL;}
		virtual ~on_demand() {release();}
		operator T*() {if (object == NULL) object = new T(); return object;}
		operator T&() {return *(T*)*this;}
		T* operator ->() {return (T*)*this;}
	};

	std::string join(const std::string& str, const std::vector<std::string>& l);
	std::vector<std::string> split(const std::string& str, const std::string delim = "", int n = 0);
	std::string trim(const std::string str);
	std::vector<std::string> map(std::string (*func)(const std::string), const std::vector<std::string> l);
}

#endif


