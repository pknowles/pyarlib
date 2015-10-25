/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <iostream>
#include <fstream>
#include <string>

#include "gpu.h"
#include "util.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifdef _WIN32
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#include "shader.h"
#include "shaderutil.h"

using namespace std;

static int64_t totalBufferMemory = 0;
static int64_t totalTextureMemory = 0;

int64_t getGPUMemoryUsage()
{
	return totalBufferMemory + totalTextureMemory;
}

unsigned int getGLReadWrite(bool read, bool write)
{
	assert(read || write);
	if (read)
	{
		if (write)
			return GL_READ_WRITE;
		else
			return GL_READ_ONLY;
	}
	return GL_WRITE_ONLY;
}

static int nextPlacementID = 0;

void GPUObject::resize()
{
	placementID = nextPlacementID++;
}

RenderTarget::RenderTarget(GLuint type) : GPUObject(type)
{
	if (type == (GLuint)-1)
		size = vec2i(-1);
	else
		size = vec2i(0);
	
	bytes = 0;
	
	attachLayer = -1;
	attachMip = 0;
}
/*
void RenderTarget::resize(vec2i size)
{
	if (size.x == 0 || size.y == 0)
	{
		release();
		return;
	}

	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	if (format == GL_DEPTH24_STENCIL8 ||
		format == GL_DEPTH_COMPONENT ||
		format == GL_DEPTH_COMPONENT16 ||
		format == GL_DEPTH_COMPONENT24 ||
		format == GL_DEPTH_COMPONENT32F)
		informat = GL_DEPTH_COMPONENT;

	switch (type)
	{
	case GL_TEXTURE_2D:
		if (object == 0)
			glGenTextures(1, &object);
		bind();		
		applyAttribsToBound();
		glTexImage2D(type, 0, format, size.x, size.y, 0, informat, GL_UNSIGNED_BYTE, NULL);
		this->size = size;
		unbind();
		break;
	case GL_RENDERBUFFER:
		if (object == 0)
			glGenRenderbuffers(1, &object);
		bind();
		glRenderbufferStorage(type, format, size.x, size.y);
		this->size = size;
		unbind();
		break;
	}
}
bool RenderTarget::release()
{
	size = vec2i(0);
	if (object == 0)
		return false;

	switch (type)
	{
	case GL_TEXTURE_2D:
		glDeleteTextures(1, &object);
		break;
	case GL_RENDERBUFFER:
		glDeleteRenderbuffers(1, &object);
		break;
	}
	object = 0;
	return true;
}
void RenderTarget::bind()
{
	switch (type)
	{
	case GL_TEXTURE_2D:
		glBindTexture(type, object);
		break;
	case GL_RENDERBUFFER:
		glBindRenderbuffer(type, object);
		break;
	}
}
void RenderTarget::unbind()
{
	switch (type)
	{
	case GL_TEXTURE_2D:
		glBindTexture(type, 0);
		break;
	case GL_RENDERBUFFER:
		glBindRenderbuffer(type, 0);
		break;
	}
}*/

TextureAttribs::TextureAttribs()
{
	mipmap = false;
	anisotropy = 0;
	nearest = false;
	repeat = false;
}

void TextureAttribs::applyAttribs(GLuint target)
{	
	CHECKERROR;
	GLuint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	GLuint mag_filter = nearest ? GL_NEAREST : GL_LINEAR;
	GLuint min_filter;
	if (mipmap)
	{
		min_filter = nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
		if (anisotropy > 1)
			glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	}
	else
		min_filter = mag_filter;
	CHECKERROR;

	if (target != GL_TEXTURE_2D_MULTISAMPLE)
	{
		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
		glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap);
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
		if (CHECKERROR)
			printf("Error: texture target cannot be filtered. Please update gpu.cpp.");
	}
	CHECKERROR;
}

bool Texture::resize(vec2i size)
{
	if (size.x == this->size.x && size.y == this->size.y)
		return false;
		
	GPUObject::resize();
	
	if (size.x == 0 || size.y == 0)
	{
		release();
		return true;
	}

	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	if (format == GL_DEPTH24_STENCIL8 ||
		format == GL_DEPTH_COMPONENT ||
		format == GL_DEPTH_COMPONENT16 ||
		format == GL_DEPTH_COMPONENT24 ||
		format == GL_DEPTH_COMPONENT32F)
		informat = GL_DEPTH_COMPONENT;

	if (object == 0)
	{
		glGenTextures(1, &object);
		if (object == 0)
		{
			printf("Could not create object (glGenTextures)\n");
			CHECKERROR;
		}
	}

	bind();
	applyAttribs(type);
	if (multisample)
	{
		CHECKERROR;
		size_t newBytes = (size_t)size.x * size.y * samples * bytesPerPixel(format);
		totalTextureMemory += newBytes - bytes;
		bytes = newBytes;
		glTexImage2DMultisample(type, samples, format, size.x, size.y, GL_FALSE);
		if (CHECKERROR)
		{
			printf("Error: Could not create multisample texture %ix%i (%ix)\n", size.x, size.y, samples);
		}
	}
	else
	{
		size_t newBytes = (size_t)size.x * size.y * bytesPerPixel(format);
		totalTextureMemory += newBytes - bytes;
		bytes = newBytes;
		glTexImage2D(type, 0, format, size.x, size.y, 0, informat, GL_UNSIGNED_BYTE, NULL);
	}
	this->size = size;
	unbind();

	CHECKERROR;
	return true;
}

size_t Texture::memoryUsage()
{
	if (multisample)
		return size.x * size.y * samples * bytesPerPixel(format);
	else
		return size.x * size.y * bytesPerPixel(format);
}

void Texture::buffer(const void* data, size_t sizeCheck)
{
	if (sizeCheck > 0)
	{
		size_t allocated = (size_t)size.x * size.y * bytesPerPixel(format);
		if (sizeCheck != allocated)
		{
			printf("Error: Texture::buffer(%i bytes) but %i allocated\n", sizeCheck, allocated);
			return;
		}
	}
	
	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	GLenum formattype = getTextureBaseType(format);
	
	if (formattype != GL_UNSIGNED_BYTE &&  formattype != GL_FLOAT)
	{
		printf("Warning: buffering unknown data type\n");
	}
	
	bind();
	glTexImage2D(type, 0, format, size.x, size.y, 0, informat, formattype, data);
	unbind();
}

bool Texture::release()
{
	size = vec2i(0);
	if (object == 0)
		return false;

	totalTextureMemory -= bytes;
	bytes = 0;
	
	glDeleteTextures(1, &object);
	object = 0;
	return true;
}
void Texture::bind() const
{
	glBindTexture(type, object);
}
void Texture::unbind() const
{
	glBindTexture(type, 0);
}
void Texture::genMipmap()
{
	bind();
	if (!mipmap)
	{
		printf("Warning: Texture::genMipmap() called but mipmap not set. Fixing.\n");
		mipmap = true;
		applyAttribs(type);
	}
	glGenerateMipmap(type);
	CHECKERROR;
	unbind();
}
int Texture::getMipmapLevels()
{
	//return 1 + ceilLog2(mymax(size.x, size.y));
	return 1 + ilog2(mymax(size.x, size.y));
}
bool Texture::setUniform(int exposeAs, Shader* program, const std::string& name) const
{
	if (!object)
	{
		printf("Error: cannot %s.set(%s). Call Texture::resize().\n", program->name().c_str(), name.c_str());
		return false;
	}
	
	GLuint loc = getLoc(program, name);
	
	if (exposeAs == Shader::SAMPLER)
	{
		if (loc == (GLuint)-1)
			return false;
		int index = program->unique("texture", name);
		glActiveTexture(GL_TEXTURE0 + index);
		bind();
		glActiveTexture(GL_TEXTURE0);
		program->set(name, index);
		//printf("Set obj_%i (loc=%i) \"%s.%s\" idx=%i\n", object, loc, program->name().c_str(), name.c_str(), index);
		return true;
	}
	else if (exposeAs == Shader::IMAGE_UNIT)
	{
		//printf("Binding %s as image\n", name.c_str());
		GLenum uniformType = getType(program, name);
		GLenum uniformSize = getSize(program, name);
		if (!matchImageUnitFormat(format, uniformType, uniformSize))
			printf("Warning: Suspected type mismatch while binding Texture as IMAGE_UNIT %s->%s\n", program->name().c_str(), name.c_str());
			
		int index = program->unique("image", name);
		glUniform1i(loc, index);
		GLenum readwrite = getGLReadWrite(true, true);
		bool layered = isTextureTypeLayered(type);
		//printf("Set %s(%i,loc=%i) index=%i layered=%i (in %s)\n", name.c_str(), object, loc, index, layered, program->name().c_str());
		glBindImageTextureEXT(index, object, 0, layered?GL_TRUE:GL_FALSE, 0, readwrite, format);
		return true;
	}
	else
	{
		printf("Error: invalid type for %s.set(%s, texture).\n", program->name().c_str(), name.c_str());
		return false;
	}
}

Texture2D::Texture2D(GLuint format) : Texture(GL_TEXTURE_2D)
{
	multisample = false;
	this->samples = 0;
	this->format = format;
}

Texture2D::Texture2D(vec2i size, GLuint format, int samples) : Texture(samples>0?GL_TEXTURE_2D_MULTISAMPLE:GL_TEXTURE_2D)
{
	multisample = samples > 0;
	this->samples = samples;
	this->format = format;
	this->size = vec2i(0);
	resize(size);
}
void Texture2D::randomize()
{
	assert(!multisample);
	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	size_t n = (size_t)size.x * size.y * cpp;
	char* dat = new char[n];
	for (size_t i = 0; i < n; ++i)
		dat[i] = rand() % 256;
	bind();
	//WARNING: could throw off texture memory usage calculations if size has changed outside resize()
	glTexImage2D(type, 0, format, size.x, size.y, 0, informat, GL_UNSIGNED_BYTE, dat);
	delete[] dat;
}

Texture3D::Texture3D(vec3i size, GLuint format, int samples, GLenum texType) : Texture(texType)
{
	multisample = samples > 0;
	this->samples = samples;
	this->format = format;
	this->layers = 0;
	this->size = vec2i(0);
	resize(size);
}

bool Texture3D::resize(vec3i size)
{
	if (size.x == this->size.x && size.y == this->size.y && size.z == layers)
		return false;
	
	if (size.x == 0 || size.y == 0 || size.z == 0)
	{
		release();
		return true;
	}
	
	layers = size.z;

	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	if (format == GL_DEPTH24_STENCIL8 ||
		format == GL_DEPTH_COMPONENT ||
		format == GL_DEPTH_COMPONENT16 ||
		format == GL_DEPTH_COMPONENT24 ||
		format == GL_DEPTH_COMPONENT32F)
		informat = GL_DEPTH_COMPONENT;

	if (object == 0)
		glGenTextures(1, &object);
	bind();
	applyAttribs(type);
	size_t newBytes = (size_t)size.x * size.y * layers * bytesPerPixel(format);
	totalTextureMemory += newBytes - bytes;
	bytes = newBytes;
	glTexImage3D(type, 0, format, size.x, size.y, layers, 0, informat, GL_UNSIGNED_BYTE, NULL);
	this->size = size;
	unbind();
	return true;
}

size_t Texture3D::memoryUsage()
{
	return size.x * size.y * layers * bytesPerPixel(format);
}

bool Texture3D::resize(vec2i size)
{
	return resize(vec3i(size, layers)); //resize, keeping the same number of layers
}

void Texture3D::randomize()
{
	assert(!multisample);
	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	size_t n = (size_t)size.x * size.y * layers * cpp;
	char* dat = new char[n];
	for (size_t i = 0; i < n; ++i)
		dat[i] = rand() % 256;
	bind();
	glTexImage3D(type, 0, format, size.x, size.y, layers, 0, informat, GL_UNSIGNED_BYTE, dat);
	delete[] dat;
}

TextureCubeMap::TextureCubeMap(vec2i size, GLuint format, int samples) : Texture(GL_TEXTURE_CUBE_MAP)
{
	multisample = samples > 0;
	this->samples = samples;
	this->format = format;
	this->size = vec2i(0);
	resize(size);
}

bool TextureCubeMap::resize(vec2i size)
{
	if (size.x == this->size.x && size.y == this->size.y)
		return false;
		
	GPUObject::resize();
	
	if (size.x == 0 || size.y == 0)
	{
		release();
		return true;
	}
	
	assert(size.x == size.y); //cubemaps must have equal dimensions

	int cpp = channelsPerPixel(format);
	GLenum informat = defaultFormat(cpp);
	if (format == GL_DEPTH24_STENCIL8 ||
		format == GL_DEPTH_COMPONENT ||
		format == GL_DEPTH_COMPONENT16 ||
		format == GL_DEPTH_COMPONENT24 ||
		format == GL_DEPTH_COMPONENT32F)
		informat = GL_DEPTH_COMPONENT;

	if (object == 0)
		glGenTextures(1, &object);

	glBindTexture(type, object);
	applyAttribs(type);
	size_t newBytes = (size_t)size.x * size.y * 6 * bytesPerPixel(format);
	totalTextureMemory += newBytes - bytes;
	bytes = newBytes;
	for (size_t i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (int)i, 0, format, size.x, size.y, 0, informat, GL_UNSIGNED_BYTE, NULL);
	}
	glBindTexture(type, 0);
	this->size = size;
	return true;
}

size_t TextureCubeMap::memoryUsage()
{
	return size.x * size.y * 6 * bytesPerPixel(format);
}

RenderBuffer::RenderBuffer(vec2i size, GLuint format, int samples) : RenderTarget(GL_RENDERBUFFER)
{
	multisample = samples > 0;
	this->samples = samples;
	this->format = format;
	this->size = vec2i(0);
	resize(size);
}

bool RenderBuffer::resize(vec2i size)
{
	if (size.x == this->size.x && size.y == this->size.y)
		return false;
		
	GPUObject::resize();
	
	if (size.x == 0 || size.y == 0)
	{
		release();
		return true;
	}

	if (object == 0)
		glGenRenderbuffers(1, &object);
	bind();
	if (multisample)
	{
		size_t newBytes = (size_t)size.x * size.y * samples * bytesPerPixel(format);
		totalTextureMemory += newBytes - bytes; //FIXME: not sure if this should be included in texture memory.
		bytes = newBytes;
		glRenderbufferStorageMultisample(type, samples, format, size.x, size.y);
	}
	else
	{
		size_t newBytes = (size_t)size.x * size.y * bytesPerPixel(format);
		totalTextureMemory += newBytes - bytes; //FIXME: not sure if this should be included in texture memory.
		bytes = newBytes;
		glRenderbufferStorage(type, format, size.x, size.y);
	}
	this->size = size;
	unbind();
	return true;
}
bool RenderBuffer::release()
{
	size = vec2i(0);
	if (object == 0)
		return false;
	
	totalTextureMemory -= bytes;
	bytes = 0;

	glDeleteRenderbuffers(1, &object);
	object = 0;
	return true;
}
void RenderBuffer::bind() const
{
	glBindRenderbuffer(type, object);
}
void RenderBuffer::unbind() const
{
	glBindRenderbuffer(type, 0);
}

//FIXME: no use having a shared pointer - the object can be re-created with release/resize anyway
GPUBuffer::GPUBuffer(GLenum type, GLenum access, bool writeable) : GPUObject(type), address(new GLuint64)
{
	*(this->address) = 0;
	this->access = access;
	this->dataSize = 0;
	this->writeable = writeable;
}
void GPUBuffer::bind() const
{
	glBindBuffer(type, object);
}
void GPUBuffer::unbind() const
{
	glBindBuffer(type, 0);
}
bool GPUBuffer::resize(size_t bytes, bool force)
{
	//NOTE: without force, buffers will ignore size reduction in favor of speed
	if (dataSize == bytes || (!force && bytes < dataSize))
		return false;
	
	GPUObject::resize();
	
	//printf("buffer %i: %.2fMB\n", object, bytes / (1024.0*1024.0));
	//assert(bytes < 700*1024*1024);
	
	if (bytes <= 0)
	{
		release();
	}
	else
	{
		if (!object) //if not allocated
		{
			assert(dataSize == 0);
			glGenBuffers(1, &object);
		}
		
		totalBufferMemory += bytes - dataSize;
		//printf("Buffers: %s (%i: %s)\n", humanBytes(totalBufferMemory).c_str(), object, humanBytes(bytes).c_str());
		
		bind();
		CHECKERROR;
		glBufferData(type, bytes, NULL, access);
		CHECKERROR;
		if (*address > 0)
		{
			glMakeBufferResidentNV(type, writeable ? GL_READ_WRITE : GL_READ_ONLY);
			glGetBufferParameterui64vNV(type, GL_BUFFER_GPU_ADDRESS_NV, address.get());
		}
		unbind();
		dataSize = bytes;
	}
	return true;
}
void GPUBuffer::buffer(const void* data, size_t bytes, size_t byteOffset)
{
	size_t maxSize = bytes + byteOffset;
	if (dataSize < maxSize)
		resize(maxSize);
		
	if (maxSize == 0)
		return; //buffer may not exist. nothing to do anyway
	
	bind();
	if (dataSize == bytes)
		glBufferData(type, bytes, data, access);
	else
		glBufferSubData(type, byteOffset, bytes, data);
	unbind();
}
void* GPUBuffer::map(bool read, bool write)
{
	if (!dataSize)
		return NULL;
	bind();
	void* ptr = glMapBuffer(type, getGLReadWrite(read, write));
	unbind();
	return ptr;
}
void* GPUBuffer::map(size_t offset, size_t size, bool read, bool write)
{
	if (!dataSize)
		return NULL;
	bind();
	void* ptr = glMapBufferRange(type, offset, size, getGLReadWrite(read, write));
	unbind();
	return ptr;
}
bool GPUBuffer::unmap()
{
	if (!dataSize)
		return true;
	bind();
	bool ok = (glUnmapBuffer(type) == GL_TRUE);
	assert(ok); //WARNING: not sure if this should always be fatal
	unbind();
	return ok;
}
void GPUBuffer::copy(GPUBuffer* dest, size_t offsetFrom, size_t offsetTo, ptrdiff_t size)
{
	if (size < 0)
		size = dataSize;
	assert(dataSize >= offsetFrom + size);
	assert(dest->dataSize >= offsetTo + size);
	glBindBuffer(GL_COPY_READ_BUFFER, object);
	glBindBuffer(GL_COPY_WRITE_BUFFER, dest->object);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, offsetFrom, offsetTo, size);
	//FIXME: ok to leave these bound?
}
size_t GPUBuffer::size()
{
	return dataSize;
}
bool GPUBuffer::release()
{
	if (!object)
		return false;
	assert(dataSize > 0);
		
	totalBufferMemory -= dataSize;
	//printf("Buffers: %s (%i: -%s)\n", humanBytes(totalBufferMemory).c_str(), object, humanBytes(dataSize).c_str());
	
	glDeleteBuffers(1, &object);
	object = 0;
	*address = 0;
	dataSize = 0;
	return true;
}
bool GPUBuffer::setUniform(int exposeAs, Shader* program, const std::string& name) const
{
	if (!object)
	{
		printf("Error: cannot %s.set(%s). Call GPUBuffer::resize().\n", program->name().c_str(), name.c_str());
		return false;
	}
	if (exposeAs == Shader::BINDLESS)
	{
		if (*address == 0)
		{
			bind();
			glMakeBufferResidentNV(type, writeable ? GL_READ_WRITE : GL_READ_ONLY);
			glGetBufferParameterui64vNV(type, GL_BUFFER_GPU_ADDRESS_NV, address.get());
			unbind();
		}
		
		//assumes buffer is to be mapped to an image unit or nvidia's bindless graphics
		GLuint loc = getLoc(program, name);
		if (loc == (GLuint)-1)
			return true; //don't report error - the shader has probably compiled out the variable
		else
		{
			glProgramUniformui64NV(*program, loc, *address);
		}
	}
	else if (exposeAs == Shader::IMAGE_UNIT)
		printf("Error: %s.set(%s) requires a TextureBuffer. Got GPUBuffer.\n", program->name().c_str(), name.c_str());
	else
		printf("Error: invalid type for %s.set(%s, GPUBuffer).\n", program->name().c_str(), name.c_str());
	return true;
}
		
TextureBuffer::TextureBuffer(GLenum fmt, GLenum access, bool writeable) : GPUBuffer(GL_TEXTURE_BUFFER, access, writeable), format(fmt)
{
	assert(fmt != GL_STATIC_DRAW); //to catch old code using the updated library
	texture = 0;
}

bool TextureBuffer::resize(size_t bytes, bool force)
{
	bool textureCreated = false;
	bool bufferResized = GPUBuffer::resize(bytes, force);
	
	//if (bytes == 0 && texture != 0)
	if (bufferResized && texture != 0) //fix for ATI: *always* delete the texture on a resize - texture buffer connection is lost otherwise
	{
		glDeleteTextures(1, &texture);
		texture = 0;
	}
	if (bytes > 0 && texture == 0)
	{
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_BUFFER, texture);
		glTexBuffer(GL_TEXTURE_BUFFER, format, object);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		
		if (CHECKERROR)
			printf("Are you using an unsupported texture format for glTexBuffer? Eg GL_RGBA should be GL_RGBA8\n");
			
		textureCreated = true;
	}
	
	return bufferResized || textureCreated;
}

bool TextureBuffer::release()
{
	GPUBuffer::release();
	
	bool wasAllocated = false;
	if (texture != 0)
	{
		glDeleteTextures(1, &texture);
		texture = 0;
		wasAllocated = true;
	}
	wasAllocated = wasAllocated || GPUBuffer::release();
	return wasAllocated;
}

bool TextureBuffer::setUniform(int exposeAs, Shader* program, const std::string& name) const
{
	if (!object)
	{
		printf("Error: cannot %s:setUniform(%s). Call TextureBuffer::resize().\n", program->name().c_str(), name.c_str());
		return false;
	}
	
	GLenum uniformType = getType(program, name);
	GLenum uniformSize = getSize(program, name);
	
	//assumes buffer is to be mapped to an image unit or nvidia's bindless graphics
	GLuint loc = getLoc(program, name);
	if (loc == (GLuint)-1)
		return true; //don't report error - the shader has probably compiled out the variable
	else
	{
		if (exposeAs == Shader::SAMPLER)
		{
			int index = program->unique("texture", name);
			glActiveTexture(GL_TEXTURE0 + index);
			glBindTexture(GL_TEXTURE_BUFFER, texture);
			glActiveTexture(GL_TEXTURE0);
			program->set(name, index);
			//printf("Set %i (loc=%i) %s %i\n", object, loc, name.c_str(), index);
			return true;
		}
		else if (exposeAs == Shader::IMAGE_UNIT)
		{
			if (!matchImageUnitFormat(format, uniformType, uniformSize))
				printf("Warning: Suspected type mismatch while binding TextureBuffer as IMAGE_UNIT %s->%s\n", program->name().c_str(), name.c_str());
			if (texture == 0)
				printf("Warning: texture binding as %s is not allocated\n", name.c_str());
				
			//NOTE: I've seen other code bind the TEXTURE_BUFFER to (ACTIVE_TEXTURE0 + index) here
			//As far as I can tell, this is not needed - "texture image units" (glActiveTexture) and image units are completely separate
			int index = program->unique("image", name);
			//printf("binding %s(%i) to image %i in %s\n", name.c_str(), texture, index, program->name().c_str());
			glUniform1i(loc, index);
			GLenum readwrite = getGLReadWrite(true, true);
			glBindImageTextureEXT(index, texture, 0, GL_FALSE, 0, readwrite, format);
			return true;
		}
		else
			return GPUBuffer::setUniform(exposeAs, program, name);
	}
}

bool TextureBuffer::setFormat(GLenum fmt)
{
	if (format != fmt)
	{
		format = fmt;
		glBindTexture(GL_TEXTURE_BUFFER, texture);
		glTexBuffer(GL_TEXTURE_BUFFER, format, object);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
		return true;
	}
	return false;
}

UniformBuffer::UniformBuffer(GLenum access, bool writeable) : GPUBuffer(GL_UNIFORM_BUFFER, access, writeable)
{
}

bool UniformBuffer::setUniform(int exposeAs, Shader* program, const std::string& name) const
{
	if (!object)
	{
		printf("Error: cannot setUniform for GPUBuffer. Call UniformBuffer::resize().\n");
		return false;
	}
	
	GLuint block = glGetUniformBlockIndex(*program, name.c_str());
	if (block == (GLuint)-1)
		return false; //return error. block handel should always exist
	else
	{
		if (exposeAs == Shader::UNIFORM_BUFFER)
		{
			int index = program->unique("ublock", name);
			glUniformBlockBinding(*program, block, index);
			glBindBufferRange(type, index, object, 0, dataSize);
			return true;
		}
		else
		{
			printf("Error: Attempting to bind UniformBuffer to invalid target\n");
			return false;
		}
	}
}

std::vector<vec4i> FrameBuffer::backupViewport;
std::vector<int> FrameBuffer::backupFBO;
	
FrameBuffer::FrameBuffer() : GPUObject(GL_FRAMEBUFFER)
{
	depth = NULL;
	stencil = NULL;

	hasResize = false;
	hasAttach = false;
	maxAttach = 0;
	
	for (int i = 0; i < 16; ++i)
		colour[i] = NULL;
	
	this->size = vec2i(0);	
	resize(size);
}
bool FrameBuffer::attach(GLenum attachment, RenderTarget* target)
{
	if (!target || !*target)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
		CHECKERROR;
		return true;
	}

	switch (target->type)
	{
	case (GLuint)-1:
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
		CHECKERROR;
		return true;
	case GL_TEXTURE_1D:
		glFramebufferTexture1D(GL_FRAMEBUFFER, attachment, target->type, *target, target->attachMip);
		break;
	case GL_TEXTURE_2D:
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, target->type, *target, target->attachMip);
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_CUBE_MAP:
	case GL_TEXTURE_2D_MULTISAMPLE:
		if (target->attachLayer < 0)
			glFramebufferTexture(GL_FRAMEBUFFER, attachment, *target, target->attachMip);
		else
			glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, *target, target->attachMip, target->attachLayer);
		break;
	case GL_RENDERBUFFER:
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, target->type, *target);
		break;
	default:
		return false;
	}
	assert(!CHECKERROR);

	//printf("%i %i == %i %i\n", size.x, size.y, target.size.x, target.size.y);
	assert(size.x == target->size.x && size.y == target->size.y);
	return true;
}
bool FrameBuffer::resize(vec2i size)
{
	GPUObject::resize(); //not that it's really needed but whatever
	
	bool childChanged = false;
	if (maxAttach == 0)
	{
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);
		maxAttach = maxAttach > 16 ? 16 : maxAttach;
	}
	
	//always resize attachments - they may have been swapped out!
	if (depth) childChanged = depth->resize(size) || childChanged;
	if (stencil) childChanged = stencil->resize(size) || childChanged;
	for (int i = 0; i < maxAttach; ++i)
		if (colour[i]) childChanged = colour[i]->resize(size) || childChanged;

	//printf("FBO Child Resized: %s\n", childChanged ? "true" : "false");

	//check if FBO is already this size
	if (size.x == this->size.x && size.y == this->size.y)
		return childChanged;

	//either release on zero, or resize/check generated on > 0
	if (size.x == 0 || size.y == 0)
		return release();
	else
	{
		if (!object)
			glGenFramebuffers(1, &object);
			
		this->size = size;
		hasResize = true;
	}
	return true;
}
void FrameBuffer::attach()
{
	if (!hasResize)
	{
		printf("Warning: FrameBuffer attach() before resize(), which creates the FBO object. Continuing anyway...\n");
		resize(size);
	}
	
	if (!object)
	{
		printf("Error: Cannot FrameBuffer::attach() - no FBO object. Call resize() with positive values\n");
		return;
	}

	CHECKERROR;
	//an opengl FBO must exist (which it will with positive FrameBuffer size) to attach()
	assert(size.x > 0 && size.y > 0);
	
	GLint currentFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, object);
	CHECKERROR;

	if (!attach(GL_DEPTH_ATTACHMENT, depth))
		printf("Error: Unknown FBO depth attachment type.\n");
CHECKERROR;
	if (!attach(GL_STENCIL_ATTACHMENT, stencil))
		printf("Error: Unknown FBO stencil attachment type.\n");
CHECKERROR;
	GLenum colourBuffers[16];
	int numColourBuffers = 0;
	for (int i = 0; i < maxAttach; ++i)
	{
		//hasColour = hasColour || (colour[i] && *colour[i]);
		if (!attach(GL_COLOR_ATTACHMENT0 + i, colour[i]))
			printf("Error: Unknown FBO colour[%i] attachment type.\n", i);
		if (colour[i])
			colourBuffers[numColourBuffers++] = GL_COLOR_ATTACHMENT0 + i;
	}

	//TODO: should glReadBuffer be changed?
	if (numColourBuffers == 0)
	{
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else
		glDrawBuffers(numColourBuffers, colourBuffers);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status)
	{
	case GL_FRAMEBUFFER_COMPLETE: /*printf("GL_FRAMEBUFFER_COMPLETE\n");*/ break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT: printf("GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT: printf("GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: printf("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT\n"); break;
	case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: printf("GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE\n"); break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: printf("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE\n"); break;
	case GL_FRAMEBUFFER_UNSUPPORTED: printf("GL_FRAMEBUFFER_UNSUPPORTED_EXT\n"); break;
	default:  printf("Unknown framebuffer status error: %i\n", status); break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, currentFBO);
	hasAttach = true;
}
void FrameBuffer::bind() const
{
	if (!hasAttach)
		printf("Error: Binding FBO without attaching. Add render targets and call attach()\n");
	if (!hasResize)
		printf("Error: Binding FBO without allocating. Call resize()\n");

	if (backupViewport.size() >= 32 || backupFBO.size() >= 32)
		printf("Error: FrameBuffer::bind() overflow\n");
	else
	{
		backupViewport.push_back(vec4i());
		backupFBO.push_back(-1);
	}
	glGetIntegerv(GL_VIEWPORT, (GLint*)&backupViewport.back());
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&backupFBO.back());
	glBindFramebuffer(GL_FRAMEBUFFER, object);
	glViewport(0, 0, size.x, size.y);
	//printf("backup %i -> %i\n", backupFBO.back(), object);
}
void FrameBuffer::unbind() const
{
	//printf("restore %i <- %i\n", backupFBO.back(), object);
	if (backupViewport.size() > 0 && backupFBO.size() > 0)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, backupFBO.back());
		glViewport(backupViewport.back().x, backupViewport.back().y, backupViewport.back().z, backupViewport.back().w);
		backupViewport.pop_back();
		backupFBO.pop_back();
	}
	else
		printf("Error: FrameBuffer::unbind() underflow\n");
}
bool FrameBuffer::release()
{
	if (object == 0)
		return false;

	if (depth) depth->release();
	if (stencil) stencil->release();
	for (int i = 0; i < maxAttach; ++i)
		if (colour[i]) colour[i]->release();
	glDeleteFramebuffers(1, &object);
	object = 0;
	size  = vec2i(0);

	return true;
}
void FrameBuffer::blit(GLuint target, bool blitDepth, vec2i offset, vec2i scaleTo)
{
	GLint currentFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);

	if (target == (GLuint)-1) //use currently bound fbo if none specified
		target = currentFBO;

	if (scaleTo.x < 0) scaleTo.x = size.x;
	if (scaleTo.y < 0) scaleTo.y = size.y;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, object);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target);
	scaleTo += offset;
	//if it crashes here, it's because there are no attachments
	/*
	if (colour[0])
		glBlitFramebuffer(0, 0, size.x, size.y, offset.x, offset.y, scaleTo.x, scaleTo.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	if (blitDepth && depth)
		glBlitFramebuffer(0, 0, size.x, size.y, offset.x, offset.y, scaleTo.x, scaleTo.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	*/
	vec2i fromsize = size;
	if (colour[0]) fromsize /= (1 << colour[0]->attachMip);
	else if (depth) fromsize /= (1 << depth->attachMip);
	int mask = (colour[0] ? GL_COLOR_BUFFER_BIT : 0) | (blitDepth && depth ? GL_DEPTH_BUFFER_BIT : 0);
	if (mask)
		glBlitFramebuffer(0, 0, fromsize.x, fromsize.y, offset.x, offset.y, scaleTo.x, scaleTo.y, mask, GL_NEAREST);
	else
		printf("Warning: Nothing to blit\n");
	//printf("%i %i -> %i %i\n", fromsize.x, fromsize.y, scaleTo.x, scaleTo.y);
	glBindFramebuffer(GL_FRAMEBUFFER, currentFBO);
	//printf("blit %i -> %i\n", object, target);
}

