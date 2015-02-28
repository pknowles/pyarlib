#version 420

#include "util.glsl"

uniform mat4 transform;

#define DRBUG_LINES 1

#if DRBUG_LINES

layout(rgba32f) uniform imageBuffer debugDataLines;

out vec4 colour;

void main()
{
	int i = gl_VertexID >> 1;
	int v = gl_VertexID & 1;
	
	vec4 from = imageLoad(debugDataLines, i*3+0); //vec4(from, 1.0)
	vec4 to = imageLoad(debugDataLines, i*3+1); //vec4(to, 1.0)
	vec4 colours = imageLoad(debugDataLines, i*3+2); //vec4(rgba8ToFloat(colFrom), rgba8ToFloat(colTo), 0, 0)
	
	colour = floatToRGBA8(colours[v & 1]);
	gl_Position = transform * ((v & 1) == 0 ? from : to);
}
#endif

