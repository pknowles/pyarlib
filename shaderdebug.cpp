
#include "prec.h"
#include "util.h"
#include "shaderdebug.h"

ShaderDebug::ShaderDebug() : counters(GL_R32UI), lines(GL_RGBA32F), events(GL_RGBA32UI), custom(GL_RGBA32F)
{
	shader = NULL;
	drawShader = NULL;
	strides[0] = 3*4*sizeof(float);
	strides[1] = 2*4*sizeof(int);
	strides[2] = 1*4*sizeof(float);
	lastSizes[0] = lastSizes[1] = lastSizes[2] = 0;
	on = true;
}
ShaderDebug::~ShaderDebug()
{
	if (shader)
	{
		shader->release();
		delete shader;
		shader = NULL;
	}
	if (drawShader)
	{
		drawShader->release();
		delete drawShader;
		drawShader = NULL;
	}
}
void ShaderDebug::define(Shader* shader)
{
	shader->define("SHADER_DEBUG", on);
}
void ShaderDebug::start(Shader* shader)
{
	if (!on)
		return;
		
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
	
	//printf("%i < %i => %i\n", lines.size(), lastSizes[1], resizeWithinFactor(lastSizes[0], lines.size()/strides[0], 1.2f) * strides[0]);
	//printf("%i\n", resizeWithinFactor(lastSizes[1], events.size()/strides[1], 1.2f) * strides[1]);
	//printf("%i\n", resizeWithinFactor(lastSizes[2], custom.size()/strides[2], 1.2f) * strides[2]);
	lines.resize(mymax(16, resizeWithinFactor(lastSizes[0], lines.size()/strides[0], 1.2f) * strides[0]));
	events.resize(mymax(16, resizeWithinFactor(lastSizes[1], events.size()/strides[1], 1.2f) * strides[1]));
	custom.resize(mymax(16, resizeWithinFactor(lastSizes[2], custom.size()/strides[2], 1.2f) * strides[2]));
	
	shader->set("debugDataLines", lines);
	shader->set("debugDataEvents", events);
	shader->set("debugDataCustom", custom);
}
void ShaderDebug::stop()
{
	if (!on)
		return;
		
	glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, counters); //FIXME: make binding point 3 not hard coded!
		
	unsigned int* vals = (unsigned int*)counters.map(true, false);
	for (int i = 0; i < 3; ++i)
		lastSizes[i] = vals[i];
	counters.unmap();
}
void ShaderDebug::draw(mat44 transform)
{
	if (!drawShader)
	{
		drawShader = new Shader("pyarlib-debug-draw");
		drawShader->define("DRAW_LINES", 1);
	}
	
	#if 0
	printf("%i\n", lastSizes[COUNTER_LINE]);
	if (lastSizes[COUNTER_LINE] > 0)
	{
		vec4f* l = (vec4f*)lines.map();
		PRINTVEC4F(l[0]);
		PRINTVEC4F(l[1]);
		lines.unmap();
	}
	#endif
	
	drawShader->use();
	drawShader->set("transform", transform);
	drawShader->set("debugDataLines", lines);
	glDrawArrays(GL_LINES, 0, myclamp(lastSizes[COUNTER_LINE] * 2, 0, 2000000));
	drawShader->unuse();
}
void ShaderDebug::bindCustom(Shader* shader)
{
}
void ShaderDebug::printEvents()
{
	//shader->existingDefines
}

