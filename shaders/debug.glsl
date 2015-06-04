
#define HAS_INCLUDED_DEBUG_SHADER //marker that allows app code to warn user when this isn't found

#define EVENT_ASSERT    1000
#define EVENT_WARNING   1001
#define EVENT_COUNT     1002

#define SHADER_DEBUG defined_by_app

#if SHADER_DEBUG

#include "util.glsl"

#if VERTEX
#define DEBUG_THREADID_X gl_VertexID
#define DEBUG_THREADID_Y 0
#elif GEOMETRY
#define DEBUG_THREADID_X gl_PrimitiveID
#define DEBUG_THREADID_Y 0
#elif FRAGMENT
#define DEBUG_THREADID_X int(gl_FragCoord.x)
#define DEBUG_THREADID_Y int(gl_FragCoord.y)
#else
#define DEBUG_THREADID_X 0
#define DEBUG_THREADID_Y 0
#endif

layout(binding = 3) uniform atomic_uint debugCounterLines;
layout(binding = 3) uniform atomic_uint debugCounterEvents;
layout(binding = 3) uniform atomic_uint debugCounterCustom;

layout(rgba32f) uniform imageBuffer debugDataLines;
layout(rgba32i) uniform iimageBuffer debugDataEvents;
layout(rgba32f) uniform imageBuffer debugDataCustom;

void debugAddLineCC(vec3 from, vec3 to, vec4 colFrom, vec4 colTo)
{
	int i = int(atomicCounterIncrement(debugCounterLines));
	imageStore(debugDataLines, i*3+0, vec4(from, 1.0));
	imageStore(debugDataLines, i*3+1, vec4(to, 1.0));
	imageStore(debugDataLines, i*3+2, vec4(rgba8ToFloat(colFrom), rgba8ToFloat(colTo), 0, 0));
}

void debugEvent(int event, ivec4 dat)
{
	int i = int(atomicCounterIncrement(debugCounterEvents));
	imageStore(debugDataEvents, i*2+0, ivec4(event, DEBUG_THREADID_X, DEBUG_THREADID_Y, 0));
	imageStore(debugDataEvents, i*2+1, dat);
}

void debugCustom(vec4 dat)
{
	int i = int(atomicCounterIncrement(debugCounterCustom));
	imageStore(debugDataCustom, i, dat);
}

#define assert(x) if (x) fireAssert(__FILE__, __LINE__)

#else
void debugEvent(int event, vec4 dat) {}
void debugAddLineCC(vec3 from, vec3 to, vec4 colFrom, vec4 colTo) {}
void debugCustom(vec4 dat) {}
#define assert(x)
#endif

void debugAddLineC(vec3 from, vec3 to, vec4 col)
{
	debugAddLineCC(from, to, col, col);
}

void debugAddLine(vec3 from, vec3 to)
{
	debugAddLineCC(from, to, vec4(1), vec4(1));
}

void fireAssert(int file, int line)
{
	debugEvent(EVENT_ASSERT, ivec4(file, line, 0, 0));
}

