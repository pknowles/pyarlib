/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>

#include <map>
#include <vector>
#include <string>
#include <set>

#include "shader.h"
#include "util.h"
#include "fileutil.h"
#include "config.h"
#include "findfile.h"

#define DEBUG_OUTPUT 0

#ifndef GL_ARB_compute_shader
#define GL_COMPUTE_SHADER 0x91B9
#endif

using namespace std;

Shader* Shader::active = NULL;
int Shader::globalCompileError = 0;

std::set<Shader*>* Shader::instances = NULL;
std::map<GLuint, Shader*> Shader::instanceLookup;
std::map<std::string, const char*>* Shader::includeOverrides;

static GLuint lastLoc = -1;

//boring code. too long to expand
template <> bool Shader::set<vec2f>(const std::string& name, const vec2f& t) {preSet(name); glUniform2f(getLoc(name), t.x, t.y); return checkSet(name, "vec2");}
template <> bool Shader::set<vec3f>(const std::string& name, const vec3f& t) {preSet(name); glUniform3f(getLoc(name), t.x, t.y, t.z); return checkSet(name, "vec3");}
template <> bool Shader::set<vec4f>(const std::string& name, const vec4f& t) {preSet(name); glUniform4f(getLoc(name), t.x, t.y, t.z, t.w); return checkSet(name, "vec4");}
template <> bool Shader::set<vec2i>(const std::string& name, const vec2i& t) {preSet(name); glUniform2i(getLoc(name), t.x, t.y); return checkSet(name, "ivec2");}
template <> bool Shader::set<vec3i>(const std::string& name, const vec3i& t) {preSet(name); glUniform3i(getLoc(name), t.x, t.y, t.z); return checkSet(name, "ivec3");}
template <> bool Shader::set<vec4i>(const std::string& name, const vec4i& t) {preSet(name); glUniform4i(getLoc(name), t.x, t.y, t.z, t.w); return checkSet(name, "ivec4");}
template <> bool Shader::set<std::vector<vec2f> >(const std::string& name, const std::vector<vec2f>& t) {preSet(name); glUniform2fv(getLoc(name), (GLsizei)t.size(), (GLfloat*)&t.front()); return checkSet(name, "vec2");}
template <> bool Shader::set<std::vector<vec3f> >(const std::string& name, const std::vector<vec3f>& t) {preSet(name); glUniform3fv(getLoc(name), (GLsizei)t.size(), (GLfloat*)&t.front()); return checkSet(name, "vec3");}
template <> bool Shader::set<std::vector<vec4f> >(const std::string& name, const std::vector<vec4f>& t) {preSet(name); glUniform4fv(getLoc(name), (GLsizei)t.size(), (GLfloat*)&t.front()); return checkSet(name, "vec4");}
template <> bool Shader::set<std::vector<vec2i> >(const std::string& name, const std::vector<vec2i>& t) {preSet(name); glUniform2iv(getLoc(name), (GLsizei)t.size(), (GLint*)&t.front()); return checkSet(name, "ivec2");}
template <> bool Shader::set<std::vector<vec3i> >(const std::string& name, const std::vector<vec3i>& t) {preSet(name); glUniform3iv(getLoc(name), (GLsizei)t.size(), (GLint*)&t.front()); return checkSet(name, "ivec3");}
template <> bool Shader::set<std::vector<vec4i> >(const std::string& name, const std::vector<vec4i>& t) {preSet(name); glUniform4iv(getLoc(name), (GLsizei)t.size(), (GLint*)&t.front()); return checkSet(name, "ivec4");}
template <> bool Shader::set<int>(const std::string& name, const int& t) {preSet(name); glUniform1i(getLoc(name), t); return checkSet(name, "int");}
template <> bool Shader::set<unsigned int>(const std::string& name, const unsigned int& t) {preSet(name); glUniform1ui(getLoc(name), t); return checkSet(name, "uint");}
template <> bool Shader::set<long unsigned int>(const std::string& name, const long unsigned int& t) {preSet(name); glUniform1ui(getLoc(name), t); return checkSet(name, "uint");}
template <> bool Shader::set<float>(const std::string& name, const float& t) {preSet(name); glUniform1f(getLoc(name), t); return checkSet(name, "float");}
template <> bool Shader::set<bool>(const std::string& name, const bool& t) {preSet(name); glUniform1i(getLoc(name), (int)t); return checkSet(name, "int");}
template <> bool Shader::set<mat44>(const std::string& name, const mat44& t) {preSet(name); glUniformMatrix4fv(getLoc(name), 1, GL_FALSE, t.m); return checkSet(name, "mat4");}
template <> bool Shader::set<mat33>(const std::string& name, const mat33& t) {preSet(name); glUniformMatrix3fv(getLoc(name), 1, GL_FALSE, t.m); return checkSet(name, "mat3");}

static std::string lastErrorString;
bool _checkGLErrors(const char* file, int line, bool silent)
{
	if (Shader::globalCompileError > 0)
		silent = true; //user already knows he's buggered up. no point rubbing it in
	
	bool ok = true;
    GLenum error, last;
	
	//get all errors
	error = glGetError();
    while (error != GL_NO_ERROR)
    {
		const char* p = file;
		while (*p != '\\' && *p != '\0') ++p;
		if (*p == '\\') file = p+1; //print only filename past the last "\"
		
		//print error
    	lastErrorString = (const char*)gluErrorString(error);
    	ok = false;
    	if (!silent)
			printf("glError 0x%x caught in %s at %i: %s\n", error, file, line, gluErrorString(error));
		
		//to stop an infinite loop
		last = error;
		error = glGetError();
		if (error == last)
		{
			printf("GL error in error function. Have you initialized opengl?\n");
			return false;
		}
    }
    return !ok;
}

bool _checkEnabledAttribs(const char* file, int line)
{
	bool found = false;
	int maxAttribs;
	//check if any attributes are enabled (eg glEnableVertexAttrib)
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
	for (int i = 0; i < maxAttribs; ++i)
	{
		int enabled;
		glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
		if (enabled)
		{
			printf("ERROR in %s:%i: Vertex attrib %i has not been disabled somewhere.\n", file, line, i);
			found = true;
		}
	}
	
	//check if any client states are enabled (eg glEnableClientState)
	GLenum arrays[] = {
		GL_VERTEX_ARRAY,
		GL_COLOR_ARRAY,
		GL_TEXTURE_COORD_ARRAY,
		GL_SECONDARY_COLOR_ARRAY,
		GL_NORMAL_ARRAY,
		GL_INDEX_ARRAY,
		GL_EDGE_FLAG_ARRAY,
		GL_FOG_COORD_ARRAY
		};
	CHECKERROR;
	for (int i = 0; i < (int)(sizeof(arrays)/sizeof(GLenum)); ++i)
	{
		int enabled;
		printf("%i %i\n", i, arrays[i]);
		glGetIntegerv(arrays[i], &enabled);
		CHECKERROR;
		if (enabled)
		{
			printf("ERROR in %s:%i: Array attrib %i has not been disabled somewhere.\n", file, line, arrays[i]);
			found = true;
		}
	}
	CHECKERROR;
	return found;
}

GLuint ShaderUniform::getLoc(Shader* program, const std::string& name) const
{
	return program->getLoc(name);
}

GLenum ShaderUniform::getType(Shader* program, const std::string& name) const
{
	std::vector<std::string> arrayBits = pyarlib::split(name, "[");
	for (size_t i = 0; i < arrayBits.size(); ++i)
		arrayBits[i] = arrayBits[i].substr(arrayBits[i].find_first_of("]")+1);
	std::string searchName = pyarlib::join("[0]", arrayBits);
	
	if (program->uniforms.find(searchName) == program->uniforms.end())
	{
		printf("Warning: %s is not a uniform in %s.\n", name.c_str(), program->name().c_str());
		return 0;
	}
	return program->uniforms[searchName].type;
}

GLint ShaderUniform::getSize(Shader* program, const std::string& name) const
{
	std::vector<std::string> arrayBits = pyarlib::split(name, "[");
	for (size_t i = 0; i < arrayBits.size(); ++i)
		arrayBits[i] = arrayBits[i].substr(arrayBits[i].find_first_of("]")+1);
	std::string searchName = pyarlib::join("[0]", arrayBits);
	
	if (program->uniforms.find(searchName) == program->uniforms.end())
	{
		printf("Warning: %s is not a uniform in %s.\n", name.c_str(), program->name().c_str());
		return 0;
	}
	return program->uniforms[searchName].size;
}

bool ShaderUniform::setUniform(Shader* program, const std::string& name) const
{
	std::vector<std::string> arrayBits = pyarlib::split(name, "[");
	for (size_t i = 0; i < arrayBits.size(); ++i)
		arrayBits[i] = arrayBits[i].substr(arrayBits[i].find_first_of("]")+1);
	std::string searchName = pyarlib::join("[0]", arrayBits);

	Shader::SetType exposeAs;
	std::map<std::string, Shader::Location>::iterator u;
	//for (u = program->uniforms.begin(); u != program->uniforms.end(); ++u)
	//	printf("%s\n", u->first.c_str());
	if ((u = program->uniforms.find(searchName)) != program->uniforms.end())
	{
		exposeAs = Shader::getUniformSetType(u->second.type);
		//printf("%s : %i\n", name.c_str(), exposeAs);
		if (exposeAs == Shader::UNKNOWN)
		{
			printf("Error: unknown uniform type for shader %s set(%s)\n", program->name().c_str(), name.c_str());
			return false;
		}
		else
		{
			program->lastAutoExposeAs = Shader::getSetTypeName(exposeAs);
			return setUniform(exposeAs, program, name);
		}
	}
	else if (program->uniformBlocks.find(searchName) != program->uniformBlocks.end())
	{
		program->lastAutoExposeAs = Shader::getSetTypeName(Shader::UNIFORM_BUFFER);
		return setUniform(Shader::UNIFORM_BUFFER, program, name);
	}
	else if (program->attributes.find(searchName) != program->attributes.end())
	{
		program->lastAutoExposeAs = Shader::getSetTypeName(Shader::ATTRIBUTE);
		return setUniform(Shader::ATTRIBUTE, program, name);
	}
	
	//would leave this in, but if a variable gets compiled out, I don't want spam
	//printf("Error: unknown type for shader %s set(%s,dat). Consider set(i,%s,dat) directly.\n", program->name().c_str(), name.c_str(), name.c_str());
	program->variableError.insert(searchName);
	
	return true; //probably just compiled out. don't need to report the error
}

Shader::Shader()
{
	pname = "<uninitialized>";
	init();
}

Shader::Shader(string filename)
{
	load(filename);
	init();
}

Shader::Shader(string vert, string frag, string geom)
{
	load(vert, frag, geom);
	init();
}

Shader::~Shader()
{
	//don't release as shaders may be declared static
	//and there's no guarantee
	//release();
	if (instanceLookup.find(program) != instanceLookup.end())
		instanceLookup.erase(program); //erase this instance if it exists in the program lookup
	
	instances->erase(this);
}

void Shader::load(string filename)
{
	pname = filename;
	vert = pname + ".vert";
	frag = pname + ".frag";
	geom = pname + ".geom";
	comp = pname + ".comp";
	dirty = true;
}

void Shader::load(string vert, string frag, string geom)
{
	//generate a name for the shader if none exists
	if (pname.size() == 0)
	{
		int i = frag.rfind(".");
		if (i > 0)
			pname = frag.substr(0, i);
		else
			pname = frag;
	}
	
	this->vert = vert;
	this->frag = frag;
	this->geom = geom;
	this->comp = "";
	dirty = true;
}

void Shader::init()
{
	compileError = false;
	program = 0;
	
	if (!instances)
		instances = new std::set<Shader*>();
	instances->insert(this);
}

GLuint Shader::getLoc(const std::string& name)
{
	GLuint loc = glGetUniformLocation(program, name.c_str());
	if (loc == (GLuint)-1)
		variableError.insert(name);
	lastLoc = loc;
	return loc;
}

void Shader::preSet(const std::string& name)
{
	if (active != this && Shader::globalCompileError == 0)
		printf("Error: shader %s is not active while setting %s\n", pname.c_str(), name.c_str());
	if (CHECKERROR && Shader::globalCompileError == 0) //error happened elsewhere
		printf("\tCaught while setting uniform %s for %s\n", name.c_str(), pname.c_str());
}

bool Shader::checkSet(const std::string& name, const std::string& type)
{
	if (CHECKERROR_SILENT && !error())
	{
		printf("Error setting uniform (%s). Is %s bound and can %s be a %s?\n", lastErrorString.c_str(), pname.c_str(), name.c_str(), type.c_str());
		return false;
	}
	return lastLoc != (GLuint)-1;
}

const std::string& Shader::name() const
{
	return pname;
}

bool Shader::error()
{
	return compileError;
}

std::string Shader::getSetTypeName(SetType setType)
{
	switch (setType)
	{
	case UNKNOWN: return "UNKNOWN";
	case VARIABLE: return "VARIABLE";
	case ATTRIBUTE: return "ATTRIBUTE";
	case SAMPLER: return "SAMPLER";
	case UNIFORM_BUFFER: return "UNIFORM_BUFFER";
	case IMAGE_UNIT: return "IMAGE_UNIT";
	case ATOMIC_COUNTER: return "ATOMIC_COUNTER";
	case BINDLESS: return "BINDLESS";
	default: return "REALLY-UNKNOWN";
	}
}
Shader::SetType Shader::getUniformSetType(GLint type)
{
	switch (type)
	{
	case GL_FLOAT:
	case GL_FLOAT_VEC2:
	case GL_FLOAT_VEC3:
	case GL_FLOAT_VEC4:
	case GL_DOUBLE:
	case GL_DOUBLE_VEC2:
	case GL_DOUBLE_VEC3:
	case GL_DOUBLE_VEC4:
	case GL_INT:
	case GL_INT_VEC2:
	case GL_INT_VEC3:
	case GL_INT_VEC4:
	case GL_UNSIGNED_INT:
	case GL_UNSIGNED_INT_VEC2:
	case GL_UNSIGNED_INT_VEC3:
	case GL_UNSIGNED_INT_VEC4:
	case GL_BOOL:
	case GL_BOOL_VEC2:
	case GL_BOOL_VEC3:
	case GL_BOOL_VEC4:
	case GL_FLOAT_MAT2:
	case GL_FLOAT_MAT3:
	case GL_FLOAT_MAT4:
	case GL_FLOAT_MAT2x3:
	case GL_FLOAT_MAT2x4:
	case GL_FLOAT_MAT3x2:
	case GL_FLOAT_MAT3x4:
	case GL_FLOAT_MAT4x2:
	case GL_FLOAT_MAT4x3:
	case GL_DOUBLE_MAT2:
	case GL_DOUBLE_MAT3:
	case GL_DOUBLE_MAT4:
	case GL_DOUBLE_MAT2x3:
	case GL_DOUBLE_MAT2x4:
	case GL_DOUBLE_MAT3x2:
	case GL_DOUBLE_MAT3x4:
	case GL_DOUBLE_MAT4x2:
	case GL_DOUBLE_MAT4x3:
		return Shader::VARIABLE;
		break;
	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE:
	case GL_SAMPLER_1D_SHADOW:
	case GL_SAMPLER_2D_SHADOW:
	case GL_SAMPLER_1D_ARRAY:
	case GL_SAMPLER_2D_ARRAY:
	case GL_SAMPLER_1D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_ARRAY_SHADOW:
	case GL_SAMPLER_2D_MULTISAMPLE:
	case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_SAMPLER_CUBE_SHADOW:
	case GL_SAMPLER_BUFFER:
	case GL_SAMPLER_2D_RECT:
	case GL_SAMPLER_2D_RECT_SHADOW:
	case GL_INT_SAMPLER_1D:
	case GL_INT_SAMPLER_2D:
	case GL_INT_SAMPLER_3D:
	case GL_INT_SAMPLER_CUBE:
	case GL_INT_SAMPLER_1D_ARRAY:
	case GL_INT_SAMPLER_2D_ARRAY:
	case GL_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_INT_SAMPLER_BUFFER:
	case GL_INT_SAMPLER_2D_RECT:
	case GL_UNSIGNED_INT_SAMPLER_1D:
	case GL_UNSIGNED_INT_SAMPLER_2D:
	case GL_UNSIGNED_INT_SAMPLER_3D:
	case GL_UNSIGNED_INT_SAMPLER_CUBE:
	case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
	case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
	case GL_UNSIGNED_INT_SAMPLER_BUFFER:
	case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
		return Shader::SAMPLER;
		break;
	case GL_IMAGE_1D:
	case GL_IMAGE_2D:
	case GL_IMAGE_3D:
	case GL_IMAGE_2D_RECT:
	case GL_IMAGE_CUBE:
	case GL_IMAGE_BUFFER:
	case GL_IMAGE_1D_ARRAY:
	case GL_IMAGE_2D_ARRAY:
	case GL_IMAGE_2D_MULTISAMPLE:
	case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
	case GL_INT_IMAGE_1D:
	case GL_INT_IMAGE_2D:
	case GL_INT_IMAGE_3D:
	case GL_INT_IMAGE_2D_RECT:
	case GL_INT_IMAGE_CUBE:
	case GL_INT_IMAGE_BUFFER:
	case GL_INT_IMAGE_1D_ARRAY:
	case GL_INT_IMAGE_2D_ARRAY:
	case GL_INT_IMAGE_2D_MULTISAMPLE:
	case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
	case GL_UNSIGNED_INT_IMAGE_1D:
	case GL_UNSIGNED_INT_IMAGE_2D:
	case GL_UNSIGNED_INT_IMAGE_3D:
	case GL_UNSIGNED_INT_IMAGE_2D_RECT:
	case GL_UNSIGNED_INT_IMAGE_CUBE:
	case GL_UNSIGNED_INT_IMAGE_BUFFER:
	case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
	case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
	case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
	case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
		return Shader::IMAGE_UNIT;
		break;
	case GL_UNSIGNED_INT_ATOMIC_COUNTER:
		return Shader::ATOMIC_COUNTER;
		break;
	default:
		return Shader::UNKNOWN;
		break;
	}
}

std::string Shader::findFile(std::string& name)
{
	/*
	//current working directory
	if (fileExsts(name.c_str()))
		return name;
	
	//defined shaders path
	std::string path = joinPath(Config::getString("root"), Config::getString("shaders");
	std::string file = fileExists(joinPath(path, name).c_str());
	if (fileExists(file.c_str()))
		return file;
	*/
	
	std::string result = FileFinder::find(name);
	
	//if it exists in includeOverrides, ShaderBuild will find it
	if (includeOverrides && includeOverrides->find(name) != includeOverrides->end())
	{
		//printf("Using internal %s\n", name.c_str());
		if (result.size())
			printf("WARNING: Shader %s is overidden internally\n", result.c_str());
		return name;
	}
	
	//check current directory or any added search paths
	if (result.size())
	{
		//printf("Using file %s\n", result.c_str());
		return result;
	}
	
	return ""; //doesn't exist, remove from list
}
	
bool Shader::reload()
{
	//even if there's a compile error, it's no longer not dirty. if dirty is set later it may be because the error has been fixed
	dirty = false;
	
	CHECKERROR;
	if (!FileFinder::addDir(Config::getString("shaders")))
		printf("Warning: Could not add shaders to search path. Check config.\n");
	
	//check the separate files exist
	vert = findFile(vert);
	frag = findFile(frag);
	geom = findFile(geom);
	comp = findFile(comp);
			
	//cout << "Shader: " << pname << " = v[" << vert << "] - f[" << frag << "] - g[" << geom << "] - c[" << comp << "]" << endl;
		
	//free current program if there is one
	release();
	
	//sometimes it happens...
	if (vert.size() + frag.size() + geom.size() + comp.size() == 0)
	{
		printf("Error: No shaders found for \"%s\"\n", pname.c_str());
		return false;
	}
	else
	{

		//compile
		errorStr.clear();
		bool ok = true;
		ShaderBuild build;
		if (includeOverrides)
		{
			for (std::map<std::string, const char*>::iterator it = includeOverrides->begin(); it != includeOverrides->end(); ++it)
			{
				//add all overrides to the builder
				build.include(it->first, it->second);
			}
		}
	
		defines["HAS_GEOMETRY"] = intToString(geom.size());
	
		ok = ok && build.compile(vert, GL_VERTEX_SHADER, &defines, &errorStr);
		ok = ok && build.compile(frag, GL_FRAGMENT_SHADER, &defines, &errorStr);
		ok = ok && build.compile(geom, GL_GEOMETRY_SHADER, &defines, &errorStr);
		ok = ok && build.compile(comp, GL_COMPUTE_SHADER, &defines, &errorStr);
	
		existingDefines.clear();
		existingDefines.insert(build.existingDefines.begin(), build.existingDefines.end());
	
		CHECKERROR;
	
		//get references and their initial timestamps
		references.clear();
		std::vector<string> refs = build.getReferences();
		for (int i = 0; i < (int)refs.size(); ++i)
		{
			//FIXME: should includeOverrides be included in references?
			//if (build.includeOverrides.find(refs[i]) != build.includeOverrides.end())
			//	continue; //don't include internal (currently embedded) files in references
		
			references.push_back(
				std::pair<std::string, int>(refs[i], fileTime(refs[i].c_str()))
				);
		}
	
		//link, if compiled
		if (ok)
		{
			program = build.link(&errorStr);
			ok = ok && (program > 0);
		}
	
		CHECKERROR;
	
		if (!ok)
		{
			std::cout << "While compiling " + pname << ":" << endl << errorStr << endl;

			#if 0
			ofstream errlog("shaders.log", ios::app);
			errlog.write(errorStr.c_str(), errorStr.size());
			errlog.close();
			#endif
		}
		
		//always cleanup
		build.cleanup();
	
		#if DEBUG_OUTPUT
		printf("Shader [%s] = %i\n", pname.c_str(), program);
		#endif
	}
	
	//return program
	if (!compileError && (program == 0)) //just failed to compile. inc ref
		++globalCompileError;
	else if (compileError && !(program == 0)) //just succeddfully compiled after a failure
		--globalCompileError; //shouldn't happen due to release() above
	
	compileError = (program == 0);
		
	if (!error())
	{
		instanceLookup[program] = this;
		fillLocations();
	}
	
	return !compileError;
}

void Shader::fillLocations()
{
	#define MAX_SHADER_VAR_NAME 256
	char name[MAX_SHADER_VAR_NAME];
	GLint namelen;
	GLint loc;
	GLint varSize;
	GLenum varType;
	
	if (program == 0)
		return;
	
	GLint numAttributes;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttributes);
	for (int i = 0; i < numAttributes; ++i)
	{
		glGetActiveAttrib(program, i, MAX_SHADER_VAR_NAME, &namelen, &varSize, &varType, name);
		loc = glGetAttribLocation(program, name);
		attributes[name].loc = loc;
		attributes[name].size = varSize;
		attributes[name].type = varType;
		attributes[name].name = name;
	}
	
	GLint numUniforms;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
	for (int i = 0; i < numUniforms; ++i)
	{
		glGetActiveUniform(program, i, MAX_SHADER_VAR_NAME, &namelen, &varSize, &varType, name);
		loc = glGetUniformLocation(program, name);
		uniforms[name].loc = loc;
		uniforms[name].size = varSize;
		uniforms[name].type = varType;
		uniforms[name].name = name;
		
		//if (uniforms[name].name == "datalfb")
		//	printf("%s %i %i %i\n", name, loc, varSize, varType);
	}
	
	GLint inShaderStage[12]; //should be 3 but get a stack corruption
	GLint numUniformBlocks;
	glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numUniformBlocks);
	for (int i = 0; i < numUniformBlocks; ++i)
	{
		glGetActiveUniformBlockName(program, i, MAX_SHADER_VAR_NAME, &namelen, name);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, &loc);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &varSize);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numUniforms);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &inShaderStage[0]);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER, &inShaderStage[1]);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &inShaderStage[3]);

		uniformBlocks[name].index = i;
		uniformBlocks[name].binding = loc;
		uniformBlocks[name].size = varSize;
		uniformBlocks[name].index = i;
		uniformBlocks[name].vertex = inShaderStage[0] != 0;
		uniformBlocks[name].geometry = inShaderStage[1] != 0;
		uniformBlocks[name].fragment = inShaderStage[2] != 0;
		uniformBlocks[name].uniforms.resize(numUniforms);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, &uniformBlocks[name].uniforms[0]);
	}
	
	/*
	//TODO: maybe? cbf now
	GLint numAtomicCounters;
	glGetProgramiv(program, GL_ACTIVE_ATOMIC_COUNTERS, &numAtomicCounters);
	for (int i = 0; i < numAtomicCounters; ++i)
	{
		glGetActiveUniformBlockName(program, i, MAX_SHADER_VAR_NAME, &namelen, name);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, &loc);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, &varSize);
		glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numUniforms);
		
		glGetActiveAtomicCounterBufferiv(program, i, 
	}
	*/
}
	
bool Shader::use()
{
	assert(attribs.size() == 0); //unuse() must be called, in order, after use()
	assert(!active); //must unuse() previous shader before use()
	
	if (dirty)
		reload();
	
	if (error())
		return false;

	active = this;
	glUseProgram(program);
	return true;
}

void Shader::attrib(std::string name, GLuint buffer, GLenum type, int sizeOfElementInBytes, int stride, int offset)
{
	if (error())
		return;

	GLuint location = glGetAttribLocation(program, name.c_str());
	if (location == (GLuint)-1)
	{
		printf("Warning: no attribute %s in %s\n", name.c_str(), pname.c_str());
		variableError.insert(name);
		return;
	}

	int bpa = 0;
	switch (type)
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		bpa = 1;
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
		bpa = 2;
		break;
	case GL_INT:
	case GL_UNSIGNED_INT:
	case GL_FLOAT:
		bpa = 4;
		break;
	case GL_DOUBLE:
		bpa = 8;
		break;
	default:
		printf("Error: invalid attribute TYPE for %s in %s\n", name.c_str(), pname.c_str());
		return;
	}
	
	assert((sizeOfElementInBytes % bpa) == 0); //size must be a multiple of sizeof(TYPE)
	
	//FIXME: WHY IS INDEX THE LOCATION???
	int index = location; //Shader::unique("attrib", name);
	
	attribs.insert(index);
	glEnableVertexAttribArray(index);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glVertexAttribPointer(index, sizeOfElementInBytes / bpa, type, GL_FALSE, stride, (GLvoid*)(intptr_t)offset);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Shader::texture(std::string name, GLuint tex, GLenum target)
{
	int index = unique("texture", name);
	glActiveTexture(GL_TEXTURE0 + index);
	glBindTexture(target, tex);
	set(name, index);
	//printf("%s:%i = %s(%i)\n", pname.c_str(), index, name.c_str(), tex);
	glActiveTexture(GL_TEXTURE0);
}

bool Shader::define(std::string name, std::string value)
{
	if (defines.find(name) != defines.end())
		if (defines[name] == value)
			return false; //no change, no need to dirty

	dirty = true;
	defines[name] = value;
	#if DEBUG_OUTPUT
	printf("%s: %s = %s\n", pname.c_str(), name.c_str(), value.c_str());
	#endif
	return true;
}

bool Shader::define(std::string name, int value)
{
	return define(name, intToString(value));
}

bool Shader::getDefine(const std::string& name) const
{
	//check override defines first
	if (defines.find(name) != defines.end())
		return true;
	//search defines found during last build
	if (existingDefines.find(name) != existingDefines.end())
		return true;
	return false;
}

bool Shader::getDefine(const std::string& name, std::string& out) const
{
	//check override defines first
	auto item = defines.find(name);
	if (item != defines.end())
	{
		out = item->second;
		return true;
	}
	//search defines found during last build
	if ((item = existingDefines.find(name)) != existingDefines.end())
	{
		out = item->second;
		return true;
	}
	return false;
}

void Shader::undef(std::string name)
{
	dirty = true;
	ShaderBuild::Defines::iterator f = defines.find(name);
	if (f != defines.end())
		defines.erase(f);
}

void Shader::undefAll()
{
	defines.clear();
}

int Shader::unique(std::string type, std::string name)
{
	std::map<std::string, UniqueType>::iterator it;
	it = uniques.find(type);
	if (it == uniques.end())
	{
		UniqueType newtype;
		newtype.next = 0;
		uniques[type] = newtype;
		it = uniques.find(type);
	}
	std::map<std::string, int>::iterator mapped;
	mapped = it->second.names.find(name);
	if (mapped == it->second.names.end())
	{
		it->second.names[name] = it->second.next++;
	}
	if (type == "texture")
	{
		static int maxSamplers = 0;
		if (maxSamplers == 0)
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxSamplers);
		if (it->second.next > maxSamplers)
			printf("Warning: Over GL_MAX_FRAGMENT_TEXTURE_IMAGE_UNITS limit.");
	}
	return it->second.names[name];
}

void Shader::include(std::string filename, const char* srcData)
{
	if (!includeOverrides)
		includeOverrides = new std::map<std::string, const char*>();
	(*includeOverrides)[filename] = srcData;
}

void Shader::unuse()
{
	for (std::set<GLuint>::iterator iter = attribs.begin(); iter != attribs.end(); ++iter)
	{
		glDisableVertexAttribArray(*iter);
	}
	attribs.clear();
	glUseProgram(0);
	active = NULL;
}

bool Shader::isModified()
{
	for (int i = 0; i < (int)references.size(); ++i)
	{
		if (fileTime(references[i].first.c_str()) > references[i].second)
			return true;
	}
	return false;
}
	
void Shader::release()
{
	//CHECKERROR;
	if (program > 0)
	{
		instanceLookup.erase(program); //erase this instance if it exists in the program lookup
	
		//printf("Releasing shader %i\n", program);
		glDeleteProgram(program);
	}
	
	if (compileError) //remove fail count if shader previously had an error
		--globalCompileError;
	compileError = false;
	program = 0;
	//CHECKERROR;
}

void Shader::releaseAll()
{
	//printf("Releasing shaders\n");
	if (instances)
		for (std::set<Shader*>::iterator it = instances->begin(); it != instances->end(); ++it)
			(*it)->release();
	//printf("Done Releasing shaders\n");
}

bool Shader::reloadModified()
{
	bool anyDirty = false;
	if (instances)
	{
		for (std::set<Shader*>::iterator it = instances->begin(); it != instances->end(); ++it)
		{
			Shader& s = **it;
			if (s.isModified())
			{
				s.reload();
				anyDirty = true;
			}
		}
	}
	return anyDirty;
}

Shader* Shader::get(GLuint program)
{
	std::map<GLuint, Shader*>::iterator found;
	found = instanceLookup.find(program);	
	if (found == instanceLookup.end())
		return NULL;
	return found->second;
}

std::string Shader::getBinary()
{
	/*
	GLint numFormats = 0;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
	GLint* formats = new GLint[numFormats];
	glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats);
	for (int i = 0; i < numFormats; ++i)
		printf("%i\n", formats[i]);
	delete[] formats;
	*/
	
	//glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	
#ifdef GL_PROGRAM_BINARY_LENGTH
	std::string ret;
	GLint len = 0;
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &len);
	if (len == 0)
		printf("Shader binary not available\n");
	ret.resize(len);
	GLint actualLen = 0;
	GLenum actualFormat = 0;
	glGetProgramBinary(program, len, &actualLen, &actualFormat, (void*)&ret[0]);
	return ret;
#else
	printf("Warning: you're probably using fail-VS which means you don't get new GL stuff. No Shader::getBinary :(\n");
	return "";
#endif
}

bool Shader::writeBinary(std::string filename)
{
	if (!program)
		reload();
	
	if (error())
		return false;
	
	std::ofstream ofile(filename.c_str(), std::ios::binary);
	std::string b = getBinary();
	for (size_t i = 0; i < b.size(); ++i)
		if ((b[i] < 32 && b[i] != 10) || b[i] > 126)
			b[i] = '#';
	ofile << b;
	ofile.flush();
	
	if ((int)b.find("lmem") > 0)
		printf("#### Shader %s uses lmem\n", pname.c_str());
	else
		printf("     Shader %s uses registers\n", pname.c_str());
	
	return true;
}


