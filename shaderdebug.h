
#ifndef SHADER_DEBUG_H
#define SHADER_DEBUG_H

#include "matrix.h"
#include "gpu.h"

//usage: include debug.glsl, then shader->define("DEBUG", 1) and call start(shader), draw, stop()
//       output draw/print functions are then available

class Shader;

class ShaderDebug
{
	int strides[3];
	int lastSizes[3];
	TextureBuffer counters;
	TextureBuffer lines;
	TextureBuffer events;
	TextureBuffer custom;
	Shader* shader;
public:
	ShaderDebug();
	void start(Shader* shader);
	void stop();
	void draw(mat44 transform);
	void bindCustom(Shader* shader);
	void printEvents();
};

#endif
