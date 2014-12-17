/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef SHADER_H
#define SHADER_H

#include <stdio.h>

#include "vec.h"
#include "matrix.h"

#include "includegl.h"

#include "shaderbuild.h"

#ifdef CHECKERROR
#undef CHECKERROR
#endif
#define CHECKERROR _checkGLErrors(__FILE__, __LINE__, false)
#define CHECKERROR_SILENT _checkGLErrors(__FILE__, __LINE__, true)

#define CHECKATTRIBS _checkEnabledAttribs(__FILE__, __LINE__)

bool _checkGLErrors(const char* file, int line, bool silent = false);
bool _checkEnabledAttribs(const char* file, int line);

class Shader;

//To allow additional uniform types, extend from this struct.
//This way, myshader.set("uniformName", myclass) can be used for a custom type
//return false on an error to report it
struct ShaderUniform
{
	friend class Shader;
protected:
	
	//passthrough getLoc. use to allow shader to append variableError if location doesn't exist
	GLuint getLoc(Shader* program, const std::string& name) const;
	GLenum getType(Shader* program, const std::string& name) const;
	GLint getSize(Shader* program, const std::string& name) const;
	
	//override one of these
	virtual bool setUniform(Shader* program, const std::string& name) const; //proxy for automatic exposeAs
	virtual bool setUniform(int exposeAs, Shader* program, const std::string& name) const {return false;}
};


//the main shader class.
//handles loading and compiling shaders from source
//also has the "set" interface for setting uniform variables
//shaders are not cached
class Shader
{
	friend bool _checkGLErrors(const char* file, int line, bool silent);
	friend struct ShaderUniform;
public:
	enum SetType {
		UNKNOWN,
		VARIABLE,
		ATTRIBUTE,
		SAMPLER,
		UNIFORM_BUFFER,
		IMAGE_UNIT,
		ATOMIC_COUNTER,
		BINDLESS,
	};
	static std::string getSetTypeName(SetType setType);
private:
	struct Location
	{
		GLint loc;
		GLint size;
		GLenum type;
		std::string name;
	};
	struct UniformBlock
	{
		GLint index;
		GLint size;
		GLint binding;
		std::string name;
		std::vector<GLint> uniforms;
		bool vertex, geometry, fragment;
	};
	struct UniqueType
	{
		int next;
		std::map<std::string, int> names;
	};
	std::string pname; //combined shader program name
	//TODO: array of shaders. there's starting to be a few
	std::string vert;
	std::string frag;
	std::string geom;
	std::string comp;
	std::set<GLuint> attribs;

public: //FIXME: TEMP
	std::map<std::string, Location> uniforms;
	std::map<std::string, Location> attributes;
	std::map<std::string, UniformBlock> uniformBlocks;
private:
	static std::set<Shader*>* instances;
	static std::map<GLuint, Shader*> instanceLookup;
	static std::map<std::string, const char*>* includeOverrides;
	std::vector<std::pair<std::string, int> > references; //all files included in shader and their timestamps
	std::map<std::string, UniqueType> uniques;
	ShaderBuild::Defines defines;
	GLuint program;
	bool dirty; //need to reload() before use()?
	bool compileError;
	GLuint getLoc(const std::string& name);
	void preSet(const std::string& name);
	bool checkSet(const std::string& name, const std::string& type = "(whatever's correct)");
	void init(); //called from constructor
	void fillLocations();
	std::string findFile(std::string& name);
	
	void operator=(const Shader& other) {}; //private - NO COPYING!
public:
	static int globalCompileError; //if any shader failed to compile. used to stop GL errors flooding output
	std::string lastAutoExposeAs;
	static Shader* active;
	ShaderBuild::Defines existingDefines;
	
	static SetType getUniformSetType(GLint type);

	Shader(); //NOTE: remember to assign filenames to shader components
	Shader(std::string filename); //constructs shader searching for filename.frag, filename.vert etc
	Shader(std::string vert, std::string frag, std::string geom = ""); //explicit files
	void load(std::string filename);
	void load(std::string vert, std::string frag, std::string geom = "");
	~Shader();
	const std::string& name() const;
	const std::string& name(const std::string& newName) {return pname = newName;}
	
	std::string errorStr; //contains concatenated errors from compiling shaders, if there were any
	std::set<std::string> variableError; //uniforms/attributes with invalid locations
	bool error(); //returns compileError
	
	//recompile the shader. called automatically on first use() or use() following define/undef
	bool reload();
	
	//IMPORTANT: call unuse on THIS object as attrib and other cleanups are done
	bool use();
	void unuse();
	
	//NOTE: size is in BYTES, just like stride and offset
	void attrib(std::string name, GLuint buffer, GLenum type, int sizeOfElementInBytes, int stride = 0, int offset = 0);

	void texture(std::string name, GLuint tex, GLenum target = GL_TEXTURE_2D);
	
	//NOTE: modifying defines requires recompile so don't use once a frame!!! returns true on change
	bool define(std::string name, std::string value);
	bool define(std::string name, int value);
	bool getDefine(const std::string& name) const;
	bool getDefine(const std::string& name, std::string& out) const;
	void undef(std::string name);
	void undefAll();
	
	//textures and image units require attach points. this is a helper function to get a next available, unused index
	int unique(std::string type, std::string name);
	
	//files that would normally be read from disk can be overridden with .include(<filename to override>, <file data>)
	//NOTE: do not free srcData before the shader is compiled (or recompiled, possibly triggered by .define())
	static void include(std::string filename, const char* srcData);
	
	//set a variable name. this clas has vec and mat overrides. for others, use the ShaderUniform interface
	//bool set(const std::string& name, const ShaderUniform*& t);
	template <typename T> bool set(const std::string& name, const T& t);
	template <typename T> bool set(int exposeAs, const std::string& name, const T& t);
	
	//this class can simply be treated as the opengl shader object
	operator const GLuint&() {return program;}
	
	bool isModified(); //returns true if any source code has new timestamps
	void release();
	static void releaseAll(); //calls .release() on all instances
	static bool reloadModified();
	
	static Shader* get(GLuint program);
	
	std::string getBinary();
	bool writeBinary(std::string filename);
};

//template <typename T> bool Shader::set(const std::string& name, const T& t)

template <> bool Shader::set<vec2f>(const std::string& name, const vec2f& t);
template <> bool Shader::set<vec3f>(const std::string& name, const vec3f& t);
template <> bool Shader::set<vec4f>(const std::string& name, const vec4f& t);
template <> bool Shader::set<vec2i>(const std::string& name, const vec2i& t);
template <> bool Shader::set<vec3i>(const std::string& name, const vec3i& t);
template <> bool Shader::set<vec4i>(const std::string& name, const vec4i& t);
template <> bool Shader::set<std::vector<vec2f> >(const std::string& name, const std::vector<vec2f>& t);
template <> bool Shader::set<std::vector<vec3f> >(const std::string& name, const std::vector<vec3f>& t);
template <> bool Shader::set<std::vector<vec4f> >(const std::string& name, const std::vector<vec4f>& t);
template <> bool Shader::set<std::vector<vec2i> >(const std::string& name, const std::vector<vec2i>& t);
template <> bool Shader::set<std::vector<vec3i> >(const std::string& name, const std::vector<vec3i>& t);
template <> bool Shader::set<std::vector<vec4i> >(const std::string& name, const std::vector<vec4i>& t);
template <> bool Shader::set<int>(const std::string& name, const int& t);
template <> bool Shader::set<unsigned int>(const std::string& name, const unsigned int& t);
template <> bool Shader::set<long unsigned int>(const std::string& name, const long unsigned int& t);
template <> bool Shader::set<float>(const std::string& name, const float& t);
template <> bool Shader::set<bool>(const std::string& name, const bool& t);
template <> bool Shader::set<mat44>(const std::string& name, const mat44& t);
template <> bool Shader::set<mat33>(const std::string& name, const mat33& t);
//template <> bool Shader::set<ShaderUniform>(const std::string& name, ShaderUniform const & t);

//no-exposeAs argument. implies there's only one way (or mostly used way) to expose data in a shader
template <typename T> 
bool Shader::set(const std::string& varname, const T& t)
{
	if (error())
		return false;
	
	preSet(varname);

	//intended for ShaderUniform children. if it crashes here, parameter is not a ShaderUniform
	const ShaderUniform* su = dynamic_cast<const ShaderUniform*>(&t);
	if (su && su->setUniform(this, varname))
	{
		if (checkSet(varname, lastAutoExposeAs))
			return true;
	}
	else if (!su)
		printf("Error setting uniform %s in %s. Could not cast to ShaderUniform* (%p)\n", varname.c_str(), pname.c_str(), su);
	else
		printf("Error setting uniform %s in %s\n", varname.c_str(), pname.c_str());
	return false;
}

//allows passing an enumerant to the structure to be exposed. useful for exposing a texture as a sampler or image unit for example
template <typename T> 
bool Shader::set(int exposeAs, const std::string& varname, const T& t)
{
	if (error())
		return false;
	
	preSet(varname);

	//intended for ShaderUniform children. if it crashes here, parameter is not a ShaderUniform
	const ShaderUniform* su = dynamic_cast<const ShaderUniform*>(&t);
	if (su && su->setUniform(exposeAs, this, varname))
	{
		if (checkSet(varname))
			return true;
	}
	else if (!su)
		printf("Error setting uniform %s in %s. Could not cast to ShaderUniform* (%p)\n", varname.c_str(), pname.c_str(), su);
	else
		printf("Error setting uniform %s in %s\n", varname.c_str(), pname.c_str());
	return false;
}

#endif
