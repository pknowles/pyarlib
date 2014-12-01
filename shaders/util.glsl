
#ifndef UTIL_H
#define UTIL_H


uint rgba8ToUInt(vec4 c)
{
	uvec4 i =clamp(uvec4(c * 255.0), 0, 255);
	return (i.x << 24U) | (i.y << 16U) | (i.z << 8U) | i.w;
}

float rgba8ToFloat(vec4 c)
{
	uint i = rgba8ToUInt(c);
	return uintBitsToFloat(i);
}

vec4 uintToRGBA8(uint i)
{
	return vec4(
		(float(i>>24U))/255.0f,
		(float((i>>16U)&0xFFU))/255.0f,
		(float((i>>8U)&0xFFU))/255.0f,
		(float(i & 0xFFU))/255.0f
		);
}

vec4 floatToRGBA8(float f)
{
	uint i = floatBitsToUint(f);
	return uintToRGBA8(i);
}

float average(vec3 c)
{
	return (c.r + c.g + c.b) / 3.0;
}

vec3 hueToRGB(float h)
{
	h = fract(h) * 6.0;
	vec3 rgb;
	rgb.r = clamp(abs(3.0 - h)-1.0, 0.0, 1.0);
	rgb.g = clamp(2.0 - abs(2.0 - h), 0.0, 1.0);
	rgb.b = clamp(2.0 - abs(4.0 - h), 0.0, 1.0);
	return rgb;
}

vec3 heat(float x)
{
	return hueToRGB(2.0/3.0-(2.0/3.0)*x);
}

vec3 debugColLog(int i)
{
	if (i == 0) return vec3(1.0);
	if (i == 1) return vec3(0.0);
	if (i == 2) return vec3(1,0.5,0);
	if (i <= 4) return vec3(0.5,0,1);
	if (i <= 8) return vec3(0,0,1);
	if (i <= 16) return vec3(0,1,1);
	if (i <= 32) return vec3(0,1,0.5);
	if (i <= 64) return vec3(1,1,0);
	if (i <= 128) return vec3(0,0.5,1);
	if (i <= 256) return vec3(0,1,0);
	if (i <= 512) return vec3(1,0,0);
	return vec3(0.5, 0.5, 0.5);
}


#endif
