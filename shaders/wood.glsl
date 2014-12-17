
#include "util.glsl"

float woodWarp(float rSeg, float offset, float angle)
{
	float a = rSeg*angle/6.28318530718;
	float ai = fract(offset-2*floor(a)/rSeg)-0.5;
	float bulgeDir = 1.0/(1+24*ai*ai);
	float af = fract(a)-0.5;
	float bulge = 1.0/(1+16*af*af) - 1.0 / (1+16/4);
	return bulgeDir * bulge;
}

vec4 wood(vec3 pos)
{
	const vec3 col1 = vec3(0.84, 0.67, 0.58); //light
	const vec3 col2 = vec3(0.71, 0.47, 0.33);
	const vec3 col3 = vec3(0.62, 0.34, 0.22); //dark
	
	//make some noise!
	float a = atan(pos.y, pos.x);
	float warp = woodWarp(3.0, pos.z*0.2, a)
		+ woodWarp(7.0, pos.z*0.5, a) * 0.3;
	
	float d = length(pos.xy);
	float dd = 2.2 + 1.0 * d + 0.05 * d * d; //distant rings are close
	dd -= 0.4 * warp * d; //peturb rings by angle and height
	float r = fract(dd); //rings
	float n = fract(sin(r * 10.0) + pos.x + pos.y + pos.z); //for grain
	r = 1-(1-r)*(1-r); //for non-linear interp. less dark wood
	r += n * n * 0.3;
	return vec4(mix(mix(col3, col2, min(1.0, r * 4)), col1, r), 1.0);
}
