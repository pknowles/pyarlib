/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


//a huge opengl wrapper. nothign special. just convenience classes to make code shorter

#ifndef GPU_MEMORY_H
#define GPU_MEMORY_H

#ifdef _WIN32
#include <memory>
#else
#include <tr1/memory>
#endif

#include "vec.h"
#include "includegl.h"
#include "shader.h"

int64_t getGPUMemoryUsage();

struct GPUObject
{
	int placementID; //allows detection of a resize - primarily to notify CUDA that the object needs remapping
	GLuint object;
	GLenum type;
	GPUObject(GLuint ntype) : placementID(0), object(0), type(ntype) {}
	virtual ~GPUObject() {}
	operator GLuint() const {return object;}
	virtual void resize();
	virtual bool release() =0;
};

struct TextureAttribs
{
	bool mipmap;
	int anisotropy;
	bool nearest;
	bool repeat;
	TextureAttribs();
	void applyAttribs(GLuint target);
};

struct RenderTarget : GPUObject
{
	int attachLayer; //for FBO layered textures, default -1 to attach 3D
	int attachMip; //for FBO, default 0
	bool multisample;
	int samples;
	GLuint format;
	vec2i size;
	int bytes;
	RenderTarget(GLuint type = (GLuint)-1);
	virtual bool resize(vec2i size) =0;
	virtual bool release() =0;
	virtual void bind() const =0;
	virtual void unbind() const =0;
};

struct Texture : RenderTarget, TextureAttribs, ShaderUniform
{
protected:
	Texture(GLuint texType) : RenderTarget(texType) {}
	virtual bool setUniform(int exposeAs, Shader* program, const std::string& name) const;
public:
	void buffer(const void* data, int sizeCheck = 0); //must previously be resized!
	virtual bool resize(vec2i size);
	virtual bool release();
	virtual void bind() const;
	virtual void unbind() const;
	virtual void genMipmap();
	virtual int getMipmapLevels();
	virtual size_t memoryUsage();
};

struct Texture2D : Texture
{
	Texture2D(GLuint format);
	Texture2D(vec2i size = vec2i(0), GLuint format = GL_RGBA, int samples = 0);
	void randomize();
};

struct Texture3D : Texture
{
	int layers;
	Texture3D(vec3i size = vec3i(0), GLuint format = GL_RGBA, int samples = 0, GLenum texType = GL_TEXTURE_3D);
	virtual bool resize(vec3i size);
	virtual bool resize(vec2i size); //resize, keeping the same number of layers
	void randomize();
	virtual size_t memoryUsage();
};

struct TextureCubeMap : Texture
{
	TextureCubeMap(vec2i size = vec2i(0), GLuint format = GL_RGBA, int samples = 0);
	virtual bool resize(vec2i size);
	virtual size_t memoryUsage();
};

struct RenderBuffer : RenderTarget
{
	RenderBuffer(vec2i size = vec2i(0), GLuint format = GL_RGBA, int samples = 0);
	virtual bool resize(vec2i size);
	virtual bool release();
	virtual void bind() const;
	virtual void unbind() const;
};

struct GPUBuffer : GPUObject, ShaderUniform
{
	std::tr1::shared_ptr<GLuint64> address;
	GLenum access;
	int dataSize;
	bool writeable;

	GPUBuffer(GLenum type, GLenum access = GL_STATIC_DRAW, bool writeable = true);
	void bind() const;
	void unbind() const;
	virtual bool resize(int bytes, bool force = true); //force can be disabled to ignore size reduction
	//NOTE: .buffer(...) will NOT reduce the memory. it will only increase if needed
	void buffer(const void* data, int bytes, int byteOffset = 0);
	void createImage(GLenum format);
	void* map(bool read = true, bool write = true);
	void* map(unsigned int offset, unsigned int size, bool read = true, bool write = true);
	void copy(GPUBuffer* dest, int offsetFrom = 0, int offsetTo = 0, int size = -1); //size -1 defaults to size = dataSize
	bool unmap();
	int size();
	virtual bool release();
protected:
	virtual bool setUniform(int exposeAs, Shader* program, const std::string& name) const;
};

struct VertexBuffer : GPUBuffer
{
	VertexBuffer(GLenum naccess = GL_STATIC_DRAW, bool nwriteable = true) : GPUBuffer(GL_ARRAY_BUFFER, naccess, nwriteable) {}
};

struct IndexBuffer : GPUBuffer
{
	IndexBuffer(GLenum naccess = GL_STATIC_DRAW, bool nwriteable = true) : GPUBuffer(GL_ELEMENT_ARRAY_BUFFER, naccess, nwriteable) {}
};

struct TextureBuffer : GPUBuffer
{
	GLenum format;
	GLuint texture;
	TextureBuffer(GLenum fmt = GL_R32UI, GLenum access = GL_STATIC_DRAW, bool writeable = true);
	virtual bool resize(int bytes, bool force = true); //force can be disabled to ignore size reduction
	virtual bool release();
	bool setFormat(GLenum fmt);
protected:
	virtual bool setUniform(int exposeAs, Shader* program, const std::string& name) const;
};

struct UniformBuffer : GPUBuffer
{
	UniformBuffer(GLenum access = GL_STATIC_DRAW, bool writeable = true);
	virtual bool setUniform(int exposeAs, Shader* program, const std::string& name) const;
};

struct FrameBuffer : GPUObject
{
	bool hasResize;
	bool hasAttach;
	int maxAttach;
	static std::vector<vec4i> backupViewport;
	static std::vector<int> backupFBO;
	vec2i size; //for error checking and convenience only
	RenderTarget *depth;
	RenderTarget *stencil;
	RenderTarget *colour[16];

	GLuint getDepth() {return depth ? (GLuint)*depth : 0;}
	GLuint getStencil() {return stencil ? (GLuint)*stencil : 0;}
	GLuint getColour(int i) {return colour[i] ? (GLuint)*colour[i] : 0;}

	FrameBuffer();
	bool attach(GLenum attachment, RenderTarget* target);
	bool resize(vec2i size);
	bool resize(int w, int h) {return resize(vec2i(w, h));}
	void attach();
	void bind() const;
	void blit(GLuint target = (GLuint)-1, bool blitDepth = false, vec2i offset = vec2i(0), vec2i scaleTo = vec2i(-1));
	void unbind() const;
	virtual bool release();
};

#endif
