/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>

#include <sstream>
#include <string>
#include <iostream>
#include <iomanip>

#include "util.h"
#include "quaternion.h"

#ifndef _WIN32
#include <unistd.h>
#include <execinfo.h>
#endif

const float pi = 3.14159265f;

int ipow(int base, int exp)
{
    int result = 1;
    while (exp)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        base *= base;
    }

    return result;
}

/*
#ifdef _WIN32
#ifndef log2
float log2(float x)
{
	static const float log2f = log(2.0f);
	return log(x) / log2f;
}
double log2(double x)
{
	static const double log2d = log(2.0);
	return log(x) / log2d;
}
#endif
#endif
*/

int ilog10(int x)
{
	if (x > 99)
		if (x < 1000000)
			if (x < 10000)
				return 3 + ((int)(x - 1000) >> 31);
			else
				return 5 + ((int)(x - 100000) >> 31);
		else
			if (x < 100000000)
				return 7 + ((int)(x - 10000000) >> 31);
			else
				return 9 + ((int)((x-1000000000)&~x) >> 31);
	else
		if (x > 9)
			return 1;
		else  
			return ((int)(x - 1) >> 31);
}

int ilog2(int64_t x)
{
	const int tab64[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5};
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
	return tab64[((uint64_t)((x - (x >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

int ilog10(int64_t n)
{
	//FIXME: this is terrible!!!
	int i = 0;
    while (n > 0)
    {
    	n /= 10;
    	++i;
    }
    return i;
}


//gotta love windows:
float myround(float x)
{
	return floor(x + 0.5f);
}
float myfract(float x)
{
	return x - floor(x);
}
int mymod(int x, int m)
{
	return ((x%m)+m)%m;
}
float mymod(float x, float m)
{
	float a = x - floor(x/m)*m;
	float b = a + m;
	return b - floor(b/m)*m;
}

#define WORDBITS (sizeof(int)*8)

unsigned int ones32(register unsigned int x)
{
	/* 32-bit recursive reduction using SWAR...
	but first step is mapping 2-bit values
	into sum of 2 1-bit values in sneaky way
	*/
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0f0f0f0f);
	x += (x >> 8);
	x += (x >> 16);
	return(x & 0x0000003f);
}

int ceilLog2(int x)
{
	register int y = (x & (x - 1));
	y |= -y;
	y >>= (WORDBITS - 1);
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return(ones32(x) - 1 - y);
}
int ilog2(int x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return(ones32(x) - 1);
}
int ceilSqrt(const int& x)
{
	int i = (int)sqrt((float)x)-1;
	while (i * i < x)
		++i;
	return i;
}
int isqrt(int x)
{
	//http://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_.28base_2.29
    int res = 0;
    int bit = 1L<<30; // The second-to-top bit is set: 1L<<30 for long
 
    // "bit" starts at the highest power of four <= the argument.
    while (bit > x)
        bit >>= 2;
 
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        }
        else
            res >>= 1;
        bit >>= 2;
    }
    return res;
}
int countBits(const int& x)
{
	int c = 0;
	for (int i = 0; i < 32; ++i)
		if ((x & (1 << i)) > 0)
			++c;
	return c;
}

bool isPowerOf2(const int& x)
{
	return ones32(x) == 1;
}

int nextPowerOf2(int x)
{
	x--;
	x = (x >> 1) | x;
	x = (x >> 2) | x;
	x = (x >> 4) | x;
	x = (x >> 8) | x;
	x = (x >> 16) | x;
	x++;
	return x;
}

int resizeWithinFactor(int x, int y, float f)
{
	assert(f >= 1.0f);
	if (x < y / (f * f) || x > y)
		return (int)(x * f);
	return y;
}

int ceil(const int& n, const int& d)
{
	return n / d + (n % d > 0 ? 1 : 0);
}

vec2i ceil(const vec2i& n, const vec2i& d)
{
	return vec2i(ceil(n.x, d.x), ceil(n.y, d.y));
}

unsigned int binomial(unsigned int n, unsigned int k)
{
	//from: http://www.luschny.de/math/factorial/FastBinomialFunction.html

	assert(k <= n);
	if ((k == 0) || (k == n)) return 1;
	if (k > n / 2) { k = n - k; }
	int fi = 0, nk = n - k;
	int rootN = (int)sqrt((double)n);
	
	static int* primes = NULL;
	static int numPrimes = 0;
	static int currentN = 0;
	
	//if (currentN < n)
	//{
		//delete[] primes;
		currentN = n+1;
		primes = new int[currentN];
		int* primesSeive = new int[currentN];
		memset(primesSeive, 0, sizeof(int) * currentN);
		numPrimes = 0;
		for (int i = 2; i < currentN; ++i)
		{
			if (primesSeive[i] == 0)
			{
				primes[numPrimes++] = i;
				for (int m = i+i; m < currentN; m += i)
					++primesSeive[m];
			}
		}
		delete[] primesSeive;
	//}
	
	for(int i = 0; i < numPrimes; i++) 
	{
		int prime = primes[i];
		if (prime > nk)
		{
			primes[fi++] = prime;
			continue;
		}

		if (prime > (int)n / 2)
		{
			continue;
		}

		if (prime > rootN)
		{
			if ((n % prime) < (k % prime))
			{
				primes[fi++] = prime;
			}
			continue;
		}

		int N = n, K = k, p = 1, r = 0;

		while (N > 0)
		{
			r = ((N % prime) < (K % prime + r)) ? 1 : 0;
			if (r == 1)
			{
				p *= prime;
			}
			N /= prime;
			K /= prime;
		}
		if (p > 1) primes[fi++] = p;
	}

	unsigned int binom = 1;
	for(int i = 0; i < fi; i++) 
		binom *= primes[i];
	
	delete[] primes;
		
	return binom;
}

int fact(int i)
{
	//FIXME: much better algorithms out there
	printf("%i\n", i);
	int x = 1;
	while (i > 1)
		x *= i--;
	printf("%i\n", x);
	return x;
}

float normalpdf(float x, float sigma)
{
	return exp(-(x*x)/(2.0f*sigma*sigma)) / (sigma*sqrt(2.0f*pi));
}

float standardpdf(float x)
{
	return exp(-(x*x)/(2.0f)) / sqrt(2.0f*pi);
}

float standardcdf(float x)
{
	/*
	FROM: http://www.hobsala.com/codes/math.html
	
	This code implements a function that calculates the 
	standard normal CDF (x), using an approximation from 
	Abromowitz and Stegun Handbook of Mathematical Functions.
	http://www.math.sfu.ca/~cbm/aands/page_932.htm 
	*/
	const float b1 =  0.319381530;
	const float b2 = -0.356563782;
	const float b3 =  1.781477937;
	const float b4 = -1.821255978;
	const float b5 =  1.330274429;
	const float p  =  0.2316419;

	if (x >= 0.0)
	{
		float t = 1.0 / (1.0 + p*x);
		return (1.0 - standardpdf(x)*t* 
			(t*(t*(t*(t*b5 + b4) + b3) + b2) + b1));
	} 
	else
	{ 
		float t = 1.0 / ( 1.0 - p * x );
		return (standardpdf(x)*t* 
			(t*(t*(t*(t*b5 + b4) + b3) + b2) + b1));
	}
}

float gaussianKernal(int x, int n)
{
	const float sd = 3.0f;
	
	//find probability for range x to x+1, dividing sd standard deviations by n
	float x1 = (((float)x / (float)n) * 2.0f - 1.0f) * sd;
	float sampleDist = (sd + sd) / (float)n;
	float x2 = x1 + sampleDist;
	
	//subtract area under curve so standardpdf outside [-sd, sd] is < 0.
	//also scale returned samples so n samples sum to 1
	float total = standardcdf(sd) - standardcdf(-sd);
	float minp = standardpdf(sd);
	float sub = minp * (sd + sd);
	total -= sub;
	return ((standardcdf(x2) - standardcdf(x1)) - (minp * sampleDist)) / total;
}

vec2f randomOnCircle(float r)
{
	float a = UNIT_RAND * pi * 2.0f;
	return vec2f(cos(a)*r, sin(a)*r);
}

vec2f randomInCircle(float r)
{
	float rr = sqrt(UNIT_RAND) * r;
	float a = UNIT_RAND * pi * 2.0f;
	return vec2f(cos(a)*r, sin(a)*rr);
}

vec3f randomOnSphere(float r)
{
	float u = UNIT_RAND;
	float v = UNIT_RAND;
	vec2f dir(acos(2.0*v - 1.0)-pi*0.5, 2.0*pi*u);
	return dir.toVec() * r;
}

vec3f randomInSphere(float r)
{
	float u = UNIT_RAND;
	float v = UNIT_RAND;
	vec2f dir(acos(2.0*v - 1.0)-pi*0.5, 2.0*pi*u);
	return dir.toVec() * sqrt(UNIT_RAND) * r;
}

void createTangents(const vec3f& normal, vec3f& u, vec3f& v)
{
	u = normal;
	if (myabs(u.x) < myabs(u.y) && myabs(u.x) < myabs(u.z))
		u.x = 1.0f;
	else if (myabs(u.y) < myabs(u.z))
		u.y = 1.0f;
	else
		u.z = 1.0f;
	v = normal.cross(u) / u.size();
	u = v.cross(normal);
}

inline void poisson2D(std::vector<vec2f>& list, float rmin, float rdisc, bool square)
{
	float cell = rmin / sqrt(2.0f);
	int dim = (int)ceil(2.0f * rdisc / cell);
	std::vector<int> grid(dim*dim, -1);
	std::vector<vec2f> active;
	
	list.push_back(randomInCircle(rmin * 0.5f));
	//list.push_back(vec2i(0.0f)); //always start with center
	grid[(int)((list.back().y + rdisc) / cell) * dim + (int)((list.back().x + rdisc) / cell)] = (int)list.size() - 1;
	active.push_back(list.back());
	
	while (active.size())
	{
		int r = rand() % active.size();
		vec2f a = active[r];
		active[r] = active.back();
		
		float bestDist = rdisc;
		vec2f bestPos;
		for (int i = 0; i < 32; ++i)
		{
			float dr = UNIT_RAND;
			vec2f n = a + randomOnCircle(rmin * (1.0 + dr * dr));
			if (square)
			{
				if (mymax(myabs(n.x), myabs(n.y)) > rdisc - rmin * 0.5f)
					continue;
			}
			else
			{
				if (n.size() > rdisc - rmin * 0.5f)
					continue;
			}
			
			float nearest = rmin;
			bool found = false;
			int avg = 1;
			for (int x = mymax(0, (int)((n.x+rdisc)/cell)-2); !found && x <= mymin(dim-1, (int)((n.x+rdisc)/cell)+2); ++x)
			{
				for (int y = mymax(0, (int)((n.y+rdisc)/cell)-2); !found && y <= mymin(dim-1, (int)((n.y+rdisc)/cell)+2); ++y)
				{
					if (grid[y*dim+x] < 0)
						continue;
					float dist = (list[grid[y*dim+x]] - n).size();
					nearest += dist;
					++avg;
					if (dist < rmin)
						found = true;
				}
			}
			nearest = myabs(nearest / avg - rmin);
			if (!found && nearest < bestDist)
			{
				bestDist = nearest;
				bestPos = n;
			}
		}
		if (bestDist == rdisc)
			active.pop_back();
		else
		{
			list.push_back(bestPos);
			grid[(int)((list.back().y + rdisc) / cell) * dim + (int)((list.back().x + rdisc) / cell)] = (int)list.size() - 1;
			active.push_back(list.back());
		}
	}
}

bool compareVecSize(const vec2f& a, const vec2f& b)
{
	return a.sizesq() < b.sizesq();
}

bool compareVecSqiareDist(const vec2f& a, const vec2f& b)
{
	return mymax(myabs(a.x), myabs(a.y)) < mymax(myabs(b.x), myabs(b.y));
}

inline void poisson2D(std::vector<vec2f>& list, int n, bool square)
{
	list.clear();
	
	if (n < 1)
		return;
	
	float rmin = 1.0f / sqrt((float)n);
	poisson2D(list, rmin, 1.0f, square);
	
	//keep only the inner n points
	if (square)
		std::sort(list.begin(), list.end(), compareVecSqiareDist);
	else
		std::sort(list.begin(), list.end(), compareVecSize);
	//printf("%i > %i\n", list.size(), n);
	assert((int)list.size() >= n);
	list.resize(mymin((int)list.size(), n));
	
	//normalize
	float m;
	if (square)
		m = mymax(myabs(list[list.size()-1].x), myabs(list[list.size()-1].y));
	else
		m = list[list.size()-1].size();
	for (int i = 0; i < (int)list.size(); ++i)
		list[i] /= m;
}

void poissonSquare(std::vector<vec2f>& list, float rmin, float width) {poisson2D(list, rmin, width * 0.5f, true);}
void poissonSquare(std::vector<vec2f>& list, int n) {poisson2D(list, n, true);}
void poissonDisc(std::vector<vec2f>& list, float rmin, float rdisc) {poisson2D(list, rmin, rdisc, false);}
void poissonDisc(std::vector<vec2f>& list, int n) {poisson2D(list, n, false);}

void poissonHemisphere(std::vector<vec3f>& list, int n)
{
	list.clear();
	std::vector<vec2f> square;
	poissonSquare(square, n);
	for (int i = 0; i < (int)square.size(); ++i)
	{
		float a = pi * (square[i].x + 1.0f);
		float r = square[i].y * 0.5f + 0.5f;
		list.push_back(vec3f(cos(a)*sqrt(r), sin(a)*sqrt(r), sqrt(1.0f-r)));
	}
}

void poissonSphere(std::vector<vec3f>& list, int n)
{
	const float totalArea = 4.0f * pi;
	float pointArea = totalArea / n;
	float approxDiscRadius = sqrt(pointArea / pi);
	float minAngle = approxDiscRadius;
	float maxAngle = 2.0f * minAngle;
	list.clear();
	std::vector<vec3f> active;
	list.push_back(randomOnSphere());
	active.push_back(list.back());
	while (active.size())
	{
		//printf("%i\n", active.size());
		int r = rand() % active.size();
		vec3f a = active[r];
		active[r] = active.back();
		
		float bestDist = maxAngle;
		vec3f bestPos;
		for (int i = 0; i < 32; ++i)
		{
			float randOffset = UNIT_RAND;
			float angle = maxAngle * (1.0 + randOffset * randOffset);
			vec3f rvec = Quat(angle, randomOnSphere()).unit() * a;
			
			//ewwww
			bool ok = true;
			float nearest = 2.0f * minAngle;
			for (int j = 0; j < (int)list.size(); ++j)
			{
				float d = rvec.dot(list[j]);
				float acd = acos(d);
				if (d > 0.0f && acd < minAngle)
				{
					ok = false;
					break;
				}
				nearest = mymin(nearest, acd);
			}
			if (ok && nearest < bestDist)
			{
				bestDist = nearest;
				bestPos = rvec;
			}
		}
		if (bestDist == maxAngle)
			active.pop_back();
		else
		{
			list.push_back(bestPos);
			active.push_back(list.back());
		}
	}
}

void reflect(vec3f &out, const vec3f &incidentVec, const vec3f &normal)
{
	out = incidentVec - normal * 2.0f * incidentVec.dot(normal);
}

bool refract(vec3f &out, const vec3f &incidentVec, const vec3f &normal, float eta)
{
	float N_dot_I = normal.dot(incidentVec);
	float k = 1.0f - eta * eta * (1.0f - N_dot_I * N_dot_I);
	if (k < 0.0f)
		return false;
	else
	{
		out = incidentVec * eta - normal * (eta * N_dot_I + sqrtf(k));
		return true;
	}
}

void fresnel(float& fReflectance, const vec3f &incidentVec, const vec3f& vNormal, float fDensity1, float fDensity2)
{
	//TODO: see Schlick’s approximation
	//http://www.codermind.com/articles/Raytracer-in-C++-Depth-of-field-Fresnel-blobs.html
	float fViewProjection = incidentVec.dot(vNormal);
	float fCosThetaI = fabsf(fViewProjection); 
	float fSinThetaI = sqrtf(1 - fCosThetaI * fCosThetaI);
	float fSinThetaT = (fDensity1 / fDensity2) * fSinThetaI;
	if (fSinThetaT * fSinThetaT > 0.9999f)
	{
	  fReflectance = 1.0f ; // pure reflectance at grazing angle
	  //fCosThetaT = 0.0f;
	}
	else
	{
	  float fCosThetaT = sqrtf(1 - fSinThetaT * fSinThetaT);
	  float fReflectanceOrtho = 
		  (fDensity2 * fCosThetaT - fDensity1 * fCosThetaI ) 
		  / (fDensity2 * fCosThetaT + fDensity1  * fCosThetaI);
	  fReflectanceOrtho = fReflectanceOrtho * fReflectanceOrtho;
	  float fReflectanceParal = 
		  (fDensity1 * fCosThetaT - fDensity2 * fCosThetaI )
		  / (fDensity1 * fCosThetaT + fDensity2 * fCosThetaI);
	  fReflectanceParal = fReflectanceParal * fReflectanceParal;

	  fReflectance =  0.5f * (fReflectanceOrtho + fReflectanceParal);
	}
}

bool intersectCylinder(const vec3f& linePos, const vec3f& lineDir, const vec3f& cylPos, const vec3f& cylDir, float cylRadius, float& t1, float& t2)
{
	vec3f AB = cylDir;
	vec3f AO = linePos - cylPos;
	vec3f AOxAB = AO.cross(AB);
	vec3f VxAB = lineDir.cross(AB);
	float ab2 = AB.dot(AB);
	float a = VxAB.dot(VxAB);
	float b = 2.0f * VxAB.dot(AOxAB);
	float c = AOxAB.dot(AOxAB) - (cylRadius * cylRadius * ab2);
	float d = b*b - 4.0f * a * c;
	if (d < 0.0f)
		return false;
	d = sqrt(d);
	t1 = (-b - d) / (2.0f * a);
	t2 = (-b + d) / (2.0f * a);
	return true;
}

bool intersectCylinder(const vec3f& linePos, const vec3f& lineDir, const vec3f& cylPos, const vec3f& cylDir, float cylRadius, float height, float& t1, float& t2)
{
	float t[4];
	if (!intersectCylinder(linePos, lineDir, cylPos, cylDir, cylRadius, t[0], t[1]))
		return false;
	
	vec3f N = cylDir.unit();
	
	float delta = lineDir.dot(N);
	vec3f AO = linePos - cylPos;
	float dist = AO.dot(N);
	
	bool startInside = dist > 0.0f && dist < height;
	
	//handle division by zero
	if (delta == 0.0f)
	{
		if (startInside)
		{
			t1 = t[0];
			t2 = t[1];
			return true;
		}
		return false;
	}
	
	//plane/cap intersection times
	t[2] = -dist / delta;
	t[3] = (height - dist) / delta;
	
	//FIXME: I'm sure this could be made more efficient
	if (t[0] < t[2] && t[0] < t[3] && t[1] < t[2] && t[1] < t[3])
		return false;
	if (t[0] > t[2] && t[0] > t[3] && t[1] > t[2] && t[1] > t[3])
		return false;
	
	//sort the times
	if (t[0] > t[1]) std::swap(t[0], t[1]);
	if (t[2] > t[3]) std::swap(t[2], t[3]);
	if (t[0] > t[2]) std::swap(t[0], t[2]);
	if (t[1] > t[3]) std::swap(t[1], t[3]);
	if (t[1] > t[2]) std::swap(t[1], t[2]);
	
	//take the middle two
	t1 = t[1];
	t2 = t[2];
	return true;
}


bool intersectSphere(const vec3f& p, const vec3f& d, float r, float& t1, float& t2)
{
	//http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
	float A = d.dot(d);
	float B = 2.0f * d.dot(p);
	float C = p.dot(p) - r * r;
	
	float dis = B * B - 4.0f * A * C;
	
	if (dis < 0.0f)
		return false;
	
	float S = sqrt(dis);	
	
	t1 = (-B - S) / (2.0f * A);
	t2 = (-B + S) / (2.0f * A);
	return true;
}

bool closestPointsLines(const vec3f& p1, const vec3f& d1, const vec3f& p2, const vec3f& d2, float& t1, float& t2)
{
	vec3f p12 = p2 - p1;
	vec3f m = d2.cross(d1);
	float mm = m.dot(m);
	if (mm == 0.0f)
		return false;
	vec3f r = p12.cross(m) / mm;
	t1 = r.dot(d2);
	t2 = r.dot(d1);
	//min distance d = abs(p12 <dot> m)/sqrt(m2)
	return true;
}

float chordArea(const vec2f& centre, const float r, const vec2f& a, const vec2f& b)
{
	vec2f ca = a - centre;
	vec2f cb = b - centre;
	float axb = ca.cross(cb);
	float adb = ca.dot(cb);
	if (axb == 0.0f && adb > 0.0f)
		return 0.0f;
	float cosa = myclamp(adb / (r * r), -1.0f, 1.0f); //clamp to catch floating point error < -1.0f
	float sina = axb / (r * r);
	float angle = sina > 0.0 ? acos(cosa) : 2.0f*pi - acos(cosa);
	float area = (angle - sina) * r * r * 0.5f;
	return area;
}

//FIXME: WARNING: still has edge cases (eg. circle at origin)
float areaCircleSquare(vec2f circle, float r, vec2f square)
{
	float discr, lower, upper;
	float firstx = 0.0f, firsty = 0.0f;
	float prevx = 0.0f, prevy = 0.0f;
	float a1 = 0.0f, a2 = 0.0f;
	float chords = 0.0f;
	bool first = true;
	
	bool inside = circle.x > 0.0f && circle.x < square.x &&
		circle.y > 0.0f && circle.y < square.y;
	
	float circleArea = pi * r * r;
	
	discr = r * r - circle.x * circle.x;
	if (discr >= 0.0) //left edge
	{
		discr = sqrt(discr);
		lower = myclamp(circle.y - discr, 0.0f, square.y); //bottom
		upper = myclamp(circle.y + discr, 0.0f, square.y); //top
		if (lower != upper)
		{
			//a1 += 0.0f * lower; //x1 * y2
			//a2 += upper * 0.0f; //y2 * x1
			firstx = 0.0f;
			firsty = upper;
			prevx = 0.0f;
			prevy = lower;
			first = false;
		}
	}
	discr = r * r - circle.y * circle.y;
	if (discr >= 0.0) //bottom edge
	{
		discr = sqrt(discr);
		lower = myclamp(circle.x - discr, 0.0f, square.x); //left
		upper = myclamp(circle.x + discr, 0.0f, square.x); //right
		if (lower != upper)
		{
			if (first)
			{
				firstx = lower;
				firsty = 0.0f;
				first = false;
			}
			else
			{
				chords += chordArea(circle, r, vec2f(prevx, prevy), vec2f(lower, 0.0f));
		
				//a1 += prevx * 0.0f; //x2 * y3
				a2 += prevy * lower; //y2 * x3
			}
			a1 += lower * 0.0f; //x3 * y4
			a2 += 0.0f * upper; //y3 * x4
			prevx = upper;
			prevy = 0.0f;
		}
	}
	discr = r * r - circle.x * circle.x + 2.0f * circle.x * square.x - square.x * square.x;
	if (discr >= 0.0) //right edge
	{
		discr = sqrt(discr);
		lower = myclamp(circle.y - discr, 0.0f, square.y); //bottom
		upper = myclamp(circle.y + discr, 0.0f, square.y); //top
		if (lower != upper)
		{
			if (first)
			{
				firstx = square.x;
				firsty = lower;
				first = false;
			}
			else
			{
				chords += chordArea(circle, r, vec2f(prevx, prevy), vec2f(square.x, lower));
			
				a1 += prevx * lower; //x_n * y_n+1
				a2 += prevy * square.x; //y_n * x_n+1
			}
			a1 += square.x * upper; //x_n+1 * y_n+2
			a2 += lower * square.x; //y_n+1 * x_n+2
			prevx = square.x;
			prevy = upper;
		}
	}
	discr = r * r - circle.y * circle.y + 2.0f * circle.y * square.y - square.y * square.y;
	if (discr >= 0.0) //top edge
	{
		discr = sqrt(discr);
		lower = myclamp(circle.x - discr, 0.0f, square.x); //left
		upper = myclamp(circle.x + discr, 0.0f, square.x); //right
		if (lower != upper)
		{
			if (first)
			{
				firstx = upper;
				firsty = square.y;
				first = false;
			}
			else
			{
				chords += chordArea(circle, r, vec2f(prevx, prevy), vec2f(upper, square.y));
				
				a1 += prevx * square.y; //x_n * y_n+1
				a2 += prevy * upper; //y_n * x_n+1
			}
			a1 += upper * square.y; //x_n+1 * y_n+2
			a2 += square.y * lower; //y_n+1 * x_n+2
			prevx = lower;
			prevy = square.y;
		}
	}
	if (!first) //complete the circle
	{
		a1 += prevx * firsty; //x_n+1 * y_n+2
		a2 += prevy * firstx; //y_n+1 * x_n+2
		
		float a = chordArea(circle, r, vec2f(prevx, prevy), vec2f(firstx, firsty));
		//FIXME: edge case where circle touches an edge from the inside is unhandled due to floating point error fix
		chords += a;
	}
	
	float insidePolygonArea = (a1 - a2) * 0.5f;
	return first ? (inside ? circleArea : 0.0f) : insidePolygonArea + chords;
}

std::string binaryToString(unsigned int x, unsigned int n)
{
	std::string r;
	if (x == 0)
		return "0";
	while (x > 0)
	{
		r = "01"[x & 1] + r;
		x >>= 1;
	}
	while (r.size() < 8)
		r = "0" + r;
	return r;
}
vec3f hueToRGB(float h)
{
	h = myfract(h) * 6.0f;
	vec3f rgb;
	rgb.x = myclamp(myabs(3.0f - h)-1.0f, 0.0f, 1.0f);
	rgb.y = myclamp(2.0f - myabs(2.0f - h), 0.0f, 1.0f);
	rgb.z = myclamp(2.0f - myabs(4.0f - h), 0.0f, 1.0f);
	return rgb;
}
vec3f interpHermite(vec3f a, vec3f aDir, vec3f b, vec3f bDir, float x)
{
	//http://cubic.org/docs/hermite.htm
	static const float hd[] = {2,-3,0,1,-2,3,0,0,1,-2,1,0,1,-1,0,0};
	static const mat44 h(hd);
	mat44 C(a, b, aDir, bDir);
	vec4f S(x*x*x, x*x, x, 1.0f);
	return S * h * C;
}
vec3f interpHermiteDeriv(vec3f a, vec3f aDir, vec3f b, vec3f bDir, float x)
{
	static const float hd[] = {2,-3,0,1,-2,3,0,0,1,-2,1,0,1,-1,0,0};
	static const mat44 h(hd);
	mat44 C(a, b, aDir, bDir);
	vec4f S(3.0f*x*x, 2.0f*x, 1.0f, 0.0f);
	return S * h * C;
}
std::string humanBytes(int64_t i, bool use_binary_prefix)
{
	//http://en.wikipedia.org/wiki/Megabyte
	//binary
	static const char* prefix_iec[] = {
		"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
		};
	//decimal
	static const char* prefix_si[] = {
		"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
		};
	//since int is used, TB and greater isn't necessary
	
	std::stringstream ret;
	ret.setf(std::ios::fixed,std::ios::floatfield);
	ret << std::setprecision(2);
	if (use_binary_prefix)
	{
		int prefix = mymin(8, ilog2(myabs(i)) / 10);
		float v = (float)i / (1 << (prefix*10));
		ret << v << prefix_iec[prefix];
	}
	else
	{
		int prefix = (i == 0) ? 0 : mymin(8, (int)(log10((float)myabs(i)) / 3));
		float v = (float)i / pow(10.0f, prefix*3);
		ret << v << prefix_si[prefix];
	}
	return ret.str();
}

std::string humanNumber(float x)
{
	return "not implemented";
}

std::string humanNumber(int x)
{
	std::string r;
	bool first = true;
	for (int d = ilog10(x)/3; d > 0; --d)
	{
		int e = ipow(10, d*3);
		int v = x / e;
		r += intToString(v, first?0:3) + ",";
		x -= v * e;
		first = false;
	}
	r += intToString(x);
	return r;
}

std::string intToString(int i)
{
	std::stringstream stream;
	stream << i;
	return stream.str();
}

std::string intToString(int i, int padding)
{
	std::stringstream stream;
	stream << std::setw(padding) << std::setfill('0') << i;
	return stream.str();
}

std::string intToString(int i, int padding, char character)
{
	std::stringstream stream;
	stream << std::setw(padding) << std::setfill(character) << i;
	return stream.str();
}

std::string floatToString(float f)
{
	std::stringstream stream;
	stream << f;
	return stream.str();
}

std::string floatToString(float f, int precision)
{
	std::stringstream stream;
	stream.precision(precision);
	stream << f;
	return stream.str();
}

int stringToInt(const std::string& s)
{
	int i;
	static std::stringstream stream;
	stream.clear();
	stream.str(s);
	stream >> i;
	if (stream.fail())
		return -1;
	return i;
}

float stringToFloat(const std::string& s)
{
	float i;
	static std::stringstream stream;
	stream.clear();
	stream.str(s);
	stream >> i;
	if (stream.fail())
		return -1.0f;
	return i;
}

void mysleep(float seconds)
{
#ifdef _WIN32
	Sleep(seconds);
#else
	usleep(seconds * 1000000.0f);
#endif
}

#ifdef _WIN32

#define CLOCK_MONOTONIC_RAW 12345

//http://fossies.org/unix/privat/lft-3.34.tar.gz:a/lft-3.34/include/win32/wingettimeofday.c
LARGE_INTEGER getFILETIMEoffset()
{
	SYSTEMTIME s;
	FILETIME f;
	LARGE_INTEGER t;

	s.wYear = 1970;
	s.wMonth = 1;
	s.wDay = 1;
	s.wHour = 0;
	s.wMinute = 0;
	s.wSecond = 0;
	s.wMilliseconds = 0;
	SystemTimeToFileTime(&s, &f);
	t.QuadPart = f.dwHighDateTime;
	t.QuadPart <<= 32;
	t.QuadPart |= f.dwLowDateTime;
	return (t);
}
//http://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
int clock_gettime_win(int X, struct timeval *tv)
{
	LARGE_INTEGER           t;
	FILETIME            f;
	double                  microseconds;
	static LARGE_INTEGER    offset;
	static double           frequencyToMicroseconds;
	static int              initialized = 0;
	static BOOL             usePerformanceCounter = 0;

	if (!initialized) {
		LARGE_INTEGER performanceFrequency;
		initialized = 1;
		usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
		if (usePerformanceCounter) {
			QueryPerformanceCounter(&offset);
			frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
		} else {
			offset = getFILETIMEoffset();
			frequencyToMicroseconds = 10.;
		}
	}
	if (usePerformanceCounter) QueryPerformanceCounter(&t);
	else {
		GetSystemTimeAsFileTime(&f);
		t.QuadPart = f.dwHighDateTime;
		t.QuadPart <<= 32;
		t.QuadPart |= f.dwLowDateTime;
	}

	t.QuadPart -= offset.QuadPart;
	microseconds = (double)t.QuadPart / frequencyToMicroseconds;
	t.QuadPart = microseconds;
	tv->tv_sec = t.QuadPart / 1000000;
	tv->tv_usec = t.QuadPart % 1000000;
	return (0);
}
#endif

float MyTimer::time()
{
	struct timeval thisTime;
	struct timeval deltaTime;
	#ifdef _WIN32
	clock_gettime_win(CLOCK_MONOTONIC_RAW, &thisTime);
	#else
	struct timespec thisTime_s;
	clock_gettime(CLOCK_MONOTONIC_RAW, &thisTime_s);
	thisTime.tv_sec = thisTime_s.tv_sec;
	thisTime.tv_usec = thisTime_s.tv_nsec / 1000;
	#endif
	deltaTime = thisTime;
	deltaTime.tv_sec -= lastTime.tv_sec;
	deltaTime.tv_usec -= lastTime.tv_usec;
	lastTime = thisTime;
	return deltaTime.tv_sec * 1000.0f + deltaTime.tv_usec / 1000.0f;
}

std::string getStackString()
{
#ifdef _WIN32
	return "{no win backtrace}";
#else
	int nptrs;
	#define SIZE 100
	void *buffer[100];
	char **strings;
	std::stringstream out;
	nptrs = backtrace(buffer, SIZE);
	strings = backtrace_symbols(buffer, nptrs);
	out << "{" << nptrs << ":";
	if (strings == NULL)
	{
		for (int i = 0; i < nptrs; i++)
		{
			if (i > 0)
				out << ">";
			out << buffer[i];
		}
	}
	else
	{
		for (int i = 0; i < nptrs; i++)
		{
			if (i > 0)
				out << ">";
			out << strings[i];
		}
		free(strings);
	}
	out << "}";
	return out.str();
#endif
}

bool isBigEndian()
{
    static union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};
    return bint.c[0] == 1; 
}

std::string format(const std::string fmt, ...)
{
	int size = 100;
	std::string str;
	va_list ap;
	while (1) {
		str.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
		va_end(ap);
		if (n > -1 && n < size) {
			str.resize(n);
			return str;
		}
		if (n > -1)
			size = n + 1;
		else
			size *= 2;
	}
	return str;
}

namespace pyarlib
{
	std::string join(const std::string& str, const std::vector<std::string>& l)
	{
		if (!l.size())
			return "";
		std::stringstream s;
		s << l[0];
		for (size_t i = 1; i < l.size(); ++i)
			s << str << l[i];
		return s.str();
	}
	std::vector<std::string> split(const std::string& str, const std::string delim, int n)
	{
		std::vector<std::string> results;
		if (delim.size())
		{
			size_t p, l = 0;
			while ((p = str.find(delim, l)) != (size_t)-1)
			{
				results.push_back(str.substr(l, p-l));
				l = p + delim.size();
				if (--n == 0)
					break;
			}
			results.push_back(str.substr(l));
		}
		else
		{
			std::string::const_iterator l, p = str.begin();
			while (true)
			{
				while (p != str.end() && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
				if (p == str.end())
					break;
				l = p;
				while (p != str.end() && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
				results.push_back(std::string(l, p));
			}
		}
		return results;
	}
	
	std::string trim(const std::string str)
	{
		static const char delims[] = " \t\n\r";
		size_t a = str.find_first_not_of(delims);
		if (a == (size_t)-1)
			return "";
		size_t b = str.find_last_not_of(delims);
		return str.substr(a, 1+b-a);
	}
	
	std::vector<std::string> map(std::string (*func)(const std::string), const std::vector<std::string> l)
	{
		std::vector<std::string> results;
		results.resize(l.size());
		for (size_t i = 0; i < l.size(); ++i)
			results[i] = func(l[i]);
		return results;
	}
}
