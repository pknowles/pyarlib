/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include "material.h"
#include "shader.h"
#include "shaderutil.h"

#include "img.h"
#include "imgpng.h"
#include "util.h"


BindableMaterial::~BindableMaterial()
{
}
	
MaterialCache* MaterialCache::instance = NULL;

MaterialCache::MaterialCache()
{
}
MaterialCache::~MaterialCache()
{
	MaterialMap::iterator it;
	for (it = mats.begin(); it != mats.end(); ++it)
	{
		delete it->second;
	}
	mats.clear();
}
bool MaterialCache::getMaterial(std::string name, Material*& out)
{
	if (instance == NULL)
		instance = new MaterialCache();
	MaterialMap::iterator it;
	if ((it = instance->mats.find(name)) != instance->mats.end())
	{
		out = it->second;
		return true;
	}
	else
	{
		Material* n = new Material();
		instance->mats[name] = n;
		out = n;
		return false;
	}
}

MaterialTexture::MaterialTexture()
{
	texture = 0;
}
MaterialTexture::~MaterialTexture()
{
	releaseUploaded();
	releaseLocal();
}
void MaterialTexture::load()
{
	releaseLocal();
	
	mipmaps.push_back(new QI::ImagePNG());
	
	if (!mipmaps.back()->loadImage(filename))
		releaseLocal();
}
void MaterialTexture::load(std::string filename)
{
	this->filename = filename;
	load();
}
void MaterialTexture::operator=(QI::Image img)
{
	releaseLocal();
	mipmaps.push_back(new QI::Image(img));
}
void MaterialTexture::generateHostMipmaps()
{
	if (!mipmaps.size())
	{
		printf("Cannot create mipmaps for %s. Image not loaded\n", filename.c_str());
		return;
	}
	
	while (mipmaps.back()->width > 1 && mipmaps.back()->height > 1)
	{
		QI::Image* last = mipmaps.back();
		mipmaps.push_back(new QI::Image(*last));
		mipmaps.back()->resize(last->width / 2, last->height / 2, last->channels);
		unsigned char* data = mipmaps.back()->data;
		int w = mipmaps.back()->width;
		int h = mipmaps.back()->height;
		int w2 = last->width;
		//int h2 = last->height;
		int channels = mipmaps.back()->channels;
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				for (int c = 0; c < channels; ++c)
				{
					int i = (y*w+x)*channels + c;
					int i2 = (y*w2*2+x*2)*channels + c;
					data[i] = (unsigned char)((
						(float)last->data[i2] +
						(float)last->data[i2 + channels] +
						(float)last->data[i2 + w2 * channels] +
						(float)last->data[i2 + (w2+1) * channels] +
						0.5f
						) / 4.0f);
				}
			}
		}
	}
}
void MaterialTexture::upload()
{
	if (!mipmaps.size())
	{
		if (filename.size())
			printf("Cannot upload %s. Image not loaded\n", filename.c_str());
		return;
	}

	releaseUploaded();
	
	mipmaps[0]->anisotropy = Material::defaultAnisotropy;
	texture = mipmaps[0]->bufferTexture();
}
void MaterialTexture::releaseUploaded()
{
	if (texture)
	{
		glDeleteTextures(1, &texture);
		texture = 0;
	}
}
void MaterialTexture::releaseLocal()
{
	for (size_t i = 0; i < mipmaps.size(); ++i)
		delete mipmaps[i];
	mipmaps.clear();
}

int Material::defaultAnisotropy = 0;

Material::Material()
{
	init();
}
Material::Material(std::string colourTexture)
{
	init();
	imgColour.load(colourTexture);
}
Material::~Material()
{
	release();
}
void Material::init()
{
	uploaded = false;
	keepLocal = false;
	colour = vec4f(1.0f);
	
	ambient = vec3f(0.0f);
	specular = vec3f(0.3f);
	reflect = vec3f(0.0f);
	transmit = vec3f(0.0f);
	density = 0.0f;
	shininess = 50.0f;
	attenuation = 1.0f;
	index = 1.0f;
	gloss = 1.0f;
	internalTexture = 0.0f;
	
	reflects = false;
	transmits = false;
	unlit = false;
}
void Material::upload(bool freeLocal)
{
	if (keepLocal)
		freeLocal = false;

	imgColour.upload();
	imgNormal.upload();
	imgSpecular.upload();
	
	if (freeLocal)
		releaseLocal();
	
	uploaded = true;
}
void Material::release()
{
	releaseLocal();
	releaseUploaded();
}
void Material::releaseLocal()
{
	imgColour.releaseLocal();
	imgNormal.releaseLocal();
	imgSpecular.releaseLocal();
}
void Material::releaseUploaded()
{
	imgColour.releaseUploaded();
	imgNormal.releaseUploaded();
	imgSpecular.releaseUploaded();
}
void Material::bind()
{
	CHECKERROR;
	
	if (!uploaded)
		upload();

	GLint program = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	
	if (program == 0)
	{
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, (float*)&ambient);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, (float*)&colour);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (float*)&specular);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
		
		if (imgColour.texture)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, imgColour.texture);
		}
	}
	else
	{
		int i = 0;
		int mask = 0;
		if (Shader::active)
		{
			if (imgColour.texture) {setActiveTexture(Shader::active->unique("texture", "texColour"), "texColour", imgColour); mask |= 1<<0;}
			if (imgNormal.texture) {setActiveTexture(Shader::active->unique("texture", "texNormal"), "texNormal", imgNormal); mask |= 1<<1;}
			if (imgSpecular.texture) {setActiveTexture(Shader::active->unique("texture", "texSpecular"), "texSpecular", imgSpecular); mask |= 1<<2;}
		}
		else
		{
			if (imgColour.texture) {setActiveTexture(i++, "texColour", imgColour); mask |= 1<<0;}
			if (imgNormal.texture) {setActiveTexture(i++, "texNormal", imgNormal); mask |= 1<<1;}
			if (imgSpecular.texture) {setActiveTexture(i++, "texSpecular", imgSpecular); mask |= 1<<2;}
		}
	
		if (program != 0)
		{
			GLuint locColour = glGetUniformLocation(program, "colourIn");
			GLuint locAmbient = glGetUniformLocation(program, "colAmbient");
			GLuint locSpecular = glGetUniformLocation(program, "colSpecular");
			GLuint locTextured = glGetUniformLocation(program, "textured");
			if (locColour != (GLuint)-1)
				glUniform4fv(locColour, 1, (float*)&colour);
			if (locAmbient != (GLuint)-1)
				glUniform3fv(locAmbient, 1, (float*)&ambient);
			if (locSpecular != (GLuint)-1)
				glUniform3fv(locSpecular, 1, (float*)&specular);
			if (locTextured != (GLuint)-1)
				glUniform1i(locTextured, mask);
		}
		//printf("%i\n", mask);
	}
	
	CHECKERROR;
}
void Material::unbind()
{

	GLint program = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	
	if (program == 0)
	{
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, (float*)&ambient);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, (float*)&colour);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (float*)&specular);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
		
		if (imgColour.texture)
		{
			glDisable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, imgColour.texture);
		}
	}
	else
	{
		GLuint locTextured = glGetUniformLocation(program, "textured");
		if (locTextured)
			glUniform1i(locTextured, 0);
	}
}
