/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#ifndef SHADER_UTIL_H
#define SHADER_UTIL_H

#include "includegl.h"
#include "vec.h"

struct mat44;

const char* getFormatString(GLenum format);
bool loadGlewExtensions();
GLenum defaultFormat(int cpp);
int bytesPerPixel(GLenum format);
int channelsPerPixel(GLenum format);
GLenum getTextureBaseType(GLenum format); //eg returns GL_FLOAT for GL_RGBA32F
GLenum getUniformBaseType(GLenum format); //eg returns GL_FLOAT for GL_IMAGE_2D
bool isTextureTypeLayered(GLenum type); //eg GL_TEXTURE_2D or GL_TEXTURE_2D_ARRAY
bool matchImageUnitFormat(GLenum imageFormat, GLenum uniformFormat, GLint size);
#define setActiveTexture(index, name, texture) _setActiveTexture(index, name, texture, GL_TEXTURE_2D, __LINE__)
void _setActiveTexture(int i, const char* name, GLuint tex, GLuint target = GL_TEXTURE_2D, int line = 0);
void genCubePerspective(mat44 modelviewProjection[6], float n, float f);
void genCubeDirections(mat44 modelview[6]);
void drawSSQuad(vec2i size);
void drawSSQuad(vec3i size);
void drawSSQuad(int w = -1, int h = -1, int layers = 1);
void drawCube();
const char* getLastShaderError();

#endif
