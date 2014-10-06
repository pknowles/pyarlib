/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#ifndef PYARLIB_MATERIAL_H
#define PYARLIB_MATERIAL_H

#include "vec.h"

namespace QI {
	struct Image;
};

//TODO: use Shader::uniq to bind textures without conflicts

struct BindableMaterial
{
	virtual void bind() =0;
	virtual void unbind() =0;
	virtual void upload(bool freeLocal) =0;
	virtual ~BindableMaterial();
};

struct Material;

class MaterialCache
{
	static MaterialCache* instance;
	typedef std::map<std::string, Material*> MaterialMap;
	MaterialMap mats;
	MaterialCache();
	virtual ~MaterialCache();
public:
	//return true if material was in cache (which means it's probably already "loaded")
	static bool getMaterial(std::string name, Material*& out);
};

struct MaterialTexture
{
	MaterialTexture();
	virtual ~MaterialTexture();
	std::string filename;
	GLuint texture;
	std::vector<QI::Image*> mipmaps;
	void load();
	void load(std::string filename);
	void operator=(QI::Image img);
	void generateHostMipmaps();
	void upload();
	void releaseUploaded();
	void releaseLocal();
	operator GLuint() {return texture;}
};

struct Material : BindableMaterial
{
	bool uploaded;
	bool keepLocal; //overrides upload(true).
	static int defaultAnisotropy;

	//bound attribs
	MaterialTexture imgColour;
	MaterialTexture imgNormal;
	MaterialTexture imgSpecular;
	vec4f colour;
	
	//this stuff is just used in the raytracer, for now
	vec3f ambient;
	vec3f specular;
	vec3f reflect;
	vec3f transmit;
	float density;
	float shininess; //specular power
	float attenuation; //density constant for transmittance
	float index; //material index of refraction
	float gloss; //surface texture, how rough/bumpy the surface is. affects reflection and internal scattering
	float internalTexture; //internal scattering offsets
	
	//this stuff is computed by the raytracer for faster lookup
	bool reflects;
	bool transmits;
	bool unlit;
	
	Material();
	Material(std::string colourTexture);
	virtual ~Material();
	
	//upload() is called on first bind() if not done manually
	virtual void upload(bool freeLocal = true);
	virtual void bind();
	virtual void unbind();
	void release();
	virtual void releaseLocal();
	virtual void releaseUploaded();
	
private:
	void init();
	
	//no copying!
	Material(const Material& other) {}
	void operator=(const Material& other) {}
};

#endif
