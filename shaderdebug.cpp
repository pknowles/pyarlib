
#include "prec.h"
#include "util.h"
#include "shaderdebug.h"

ShaderDebug::ShaderDebug()
{
	shader = NULL;
	strides[0] = 3*4*sizeof(float);
	strides[1] = 2*4*sizeof(int);
	strides[2] = 1*4*sizeof(float);
	lastSizes[0] = lastSizes[1] = lastSizes[2] = 16;
}
void ShaderDebug::start(Shader* shader)
{
	if (!shader->getDefine("HAS_INCLUDED_DEBUG_SHADER"))
	{
		printf("Error: ShaderDebug::start() without including debug.glsl\n");
		this->shader = NULL;
		return;
	}
	
	this->shader = shader;
	
	//zero counters
	counters.resize(sizeof(unsigned int) * 3);
	unsigned int* vals = (unsigned int*)counters.map(false, true);
	vals[0] = vals[1] = vals[2] = 0;
	counters.unmap();
	
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, counters); //FIXME: make binding point 3 not hard coded!
	
	lines.resize(resizeWithinFactor(lastSizes[0], lines.size()/strides[0], 1.2f) * strides[0]);
	lines.resize(resizeWithinFactor(lastSizes[1], lines.size()/strides[1], 1.2f) * strides[1]);
	lines.resize(resizeWithinFactor(lastSizes[2], lines.size()/strides[2], 1.2f) * strides[2]);
	
	shader->set("debugDataLines", lines);
	shader->set("debugDataEvents", events);
	shader->set("debugDataCustom", custom);
}
void ShaderDebug::stop()
{
	unsigned int* vals = (unsigned int*)counters.map(true, false);
	for (int i = 0; i < 3; ++i)
		lastSizes[i] = vals[i];
	counters.unmap();
}
void ShaderDebug::draw(mat44 transform)
{
}
void ShaderDebug::bindCustom(Shader* shader)
{
}
void ShaderDebug::printEvents()
{
	//shader->existingDefines
}
