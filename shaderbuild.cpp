/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include "includegl.h"
#include "shaderbuild.h"
#include "fileutil.h"
#include "util.h"
#include "findfile.h"

bool ShaderBuild::printProcessed = false;

bool extractInclude(std::string& line, std::string& include)
{
	static const std::string includeStr("include");
	
	//could use boost::regex but don't want to have to add yet another lib to the deps
	std::string::iterator it = line.begin();
	
	//remove whitespace before #
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	if (*it != '#')
		return false;
	++it;
	
	//whitespace after #
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	
	//the include word
	if (line.compare(it-line.begin(), includeStr.size(), includeStr) != 0)
		return false;
	it += includeStr.size();
	
	//whitespace before "
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	if (*it != '"')
		return false;
	++it;
	
	//the filename
	include.clear();
	while (*it != '"' && it != line.end())
		include += *(it++);
		
	//closing "
	if (*it != '"')
		return false;
	return true;
}
int extractDefine(std::string& line, std::string& define)
{
	static const std::string defineStr("define");
	
	//could use boost::regex but don't want to have to add yet another lib to the deps
	std::string::iterator it = line.begin();
	
	//remove whitespace before #
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	if (*it != '#')
		return -1;
	++it;
	
	//whitespace after #
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	
	//the define word
	if (line.compare(it-line.begin(), defineStr.size(), defineStr) != 0)
		return -1;
	it += defineStr.size();
	
	//whitespace before word
	while ((*it == ' ' || *it == '\t') && it != line.end()) ++it;
	
	//the filename
	define.clear();
	while (it != line.end() && *it != ' ')
		define += *(it++); //everything until space or newline

	return (it - line.begin());
}

std::string printPath(std::vector<std::string> path)
{
	//prints the include stack
	std::string ret;
	if (path.size() > 1)
	{
		ret += "Included from:\n";
		for (int i = path.size() - 2; i >= 0; --i)
			ret += "\t" + path[i] + "\n";
	}
	return ret;
}

int extractInt(const std::string& s, std::string& r)
{
	//searches and returns the first found decimal string
	r = "";
	const char* it = &(s.c_str()[0]);
	
	//remove non-decimals
	while ((*it < '0' || *it > '9') && *it != '\0' && *it != ':')
		++it;
	if (*it < '0' || *it > '9')
		return -1;
	
	//read decimals
	while (*it >= '0' && *it <= '9' && *it != '\0')
		r += *(it++);
	
	return it - &s.c_str()[0];
}

bool ShaderBuild::getFileLine(std::string file, int line, std::string& source)
{
	line -= 1; //line numbers provided by #line are off by 1
	
	if (allfiles.find(file) != allfiles.end())
	{
		Lines& lines = allfiles[file];
		if (line >= 0 && line < (int)lines.size())
		{
			source = lines[line];
			return true;
		}
	}
	return false;

/*
	std::string ret;
	//TODO: this is plain dumb but it's 3am
	std::ifstream ifile(file.c_str());
	while (line-- > 0 && ifile.good())
		getline(ifile, ret);
	if (!ifile.good())
		ret = " <error reading source>";
	ifile.close();
	return ret;
*/
}

void ShaderBuild::parseLog(std::string* errStr, std::string log, int baseFile, bool readLineNumbers)
{
	std::string line, first, second;
	std::stringstream reader(log);
	int prevFile = -1;
	int prevLine = -1;
	while (getline(reader, line))
	{
		//attempt to read two ints - file ane line numbers
		int end = extractInt(line, first);
		if (readLineNumbers && end > 0)
			end = extractInt(line.substr(end), second);
		
		//if found, replace the error line with the file info
		if (end > 0)
		{
			//read/convert line numbers
			int filenum = stringToInt(first);
			//I could swear nvidia's compiler is reporting bad line numbers when #line is used
			int linenum = stringToInt(second);
			
			//stupid ATI drivers don't like #line before #version, so the root file never has a #line
			//this workaround replaces file 0 with the REAL file. thanks, AMD. real helpful
			if (filenum == 0)
				filenum = baseFile;
			
			//extract error message
			end = line.find(":", end);
			std::string errormsg;
			if (end >= 0)
				errormsg = line.substr(end + 1);
			else
				errormsg = line;
			
			//add file/line number info if available
			line = "";
			if (filenum >= 0 && filenum < (int)allfilebits.size())
				line += allfilebits[filenum].filename + ":";
			else
				line += "<unknown file " + intToString(filenum) + ">";
			if (readLineNumbers && linenum >= 0) line += intToString(linenum) + ":";
			
			//if both are available and valid, attempt to include the source line from the file
			int tabw = line.size();
			std::string faultline;
			if (filenum >= 0 && linenum >= 0 && filenum < (int)allfilebits.size())
			{
				std::string sourceLine;
				if (!getFileLine(allfilebits[filenum].filename, linenum, sourceLine))
					sourceLine = "<I'm stupid and can't read the source>";
				if (sourceLine.size() == 0)
					sourceLine = "<empty line>";
				faultline = " " + sourceLine;
			}
			else
				faultline = " <line not given>";
			line += faultline;
			
			//add the actual error message to the end
			if  (prevLine == linenum && prevFile == filenum)
				line = std::string(tabw, ' ') + errormsg;
			else
				line += "\n" + std::string(tabw, ' ') + errormsg;
			prevLine = linenum;
			
			//if this is the first time the file was mentioned, give the include path
			//FIXME: this is incorrect. each occurance of an include should be marked
			//       as a separate file so as to distinguish the include path!!!
			if (prevFile != filenum)
			{
				if (filenum >= 0 && filenum < (int)allfilebits.size())
					line = printPath(allfilebits[filenum].path) + line;
				else
					line = "<error in file number>\n" + line;
				prevFile = filenum;
			}
		}
		
		//print the line
		if (errStr)
			*errStr += line + "\n";
		else
			std::cout << line << std::endl;
	}
}

bool ShaderBuild::parse(std::string file, FileBits& sourceBits, Defines& defs, std::vector<std::string> path)
{
	int fileBitStart = sourceBits.size();
	
	//maintain include stack
	path.push_back(file);

	//detect infinite include
	for (int i = path.size() - 1; i >= 0; --i)
	{
		int f = path[i].find(":");
		if (f > 0 && path[i].substr(0,f) == file)
		{
			std::cout << printPath(path);
			std::cout << "Error: " << "Include loop" << ": " << file << std::endl;
			return false;
		}
	}
	
	//if the file has already been parsed, simply append the references to the bits we already have
	//don't add to the cache if #defines haven't been injected yet
	if (!firstContentLine && parseCache.find(file) != parseCache.end())
	{
		sourceBits.insert(sourceBits.end(), parseCache[file].begin(), parseCache[file].end());
		return true;
	}

	//buffer from memory or file
	std::streambuf* ifileBuff;

	//check if the source has already been supplied
	std::map<std::string, const char*>::iterator override;
	override = includeOverrides.find(file);
	std::istringstream ifileFromStr;
	std::ifstream ifileFromDisk;
	if (override != includeOverrides.end())
	{
		ifileFromStr.str(override->second);
		ifileBuff = ifileFromStr.rdbuf();
	}
	else
	{
		//open file from disk
		ifileFromDisk.open(file.c_str());
		if (!ifileFromDisk.good())
		{
			std::cout << printPath(path);
			std::cout << "Error: " << "Could not open file" << ": \"" << file << "\"" << std::endl;
			printPath(path);
			return false;
		}
		ifileBuff = ifileFromDisk.rdbuf();
	}
	
	//create reader from buffer pointer
	std::istream ifile(ifileBuff);
	
	//start caching parsed file
	allfiles[file] = Lines();
	
	//read line by line, removing comments
	int linenum = 0;
	std::string line;
	std::string include;
	std::string define;
	FileBit bit;
	bit.path = path;
	bit.filename = file;
	bit.startLine = linenum;
	while (readUncomment(ifile, line))
	{
		allfiles[file].push_back(line.substr(0, line.size()-1));
		
		//if ((int)line.find("while") >= 0)
		//	printf("WARNING: while loop found in %s.\n", file.c_str());
	
		//replace #define macro if found
		int end = extractDefine(line, define);
		if (end >= 0)
		{
			existingDefines[define] = pyarlib::trim(stripComments(line.substr(end)));
			for (Defines::iterator it = defs.begin(); it != defs.end(); ++it)
			{
				if (it->first == define)
				{
					line = line.substr(0, end+1) + it->second + "\n";
				}
			}
		}
		
		if (extractInclude(line, include))
		{
			//replace macros in include line
			if (defs.find(include) != defs.end())
				include = defs[include];
			
			if (include != "<none>") //Special name to ignore the include
			{
				//add current bit
				int bitIndex = allfilebits.size();
				if (sourceBits.size() > 0) //ATI doesn't like #line before #version
					bit.bit = "#line " + intToString(bit.startLine+1) + " " + intToString(bitIndex) + "\n" + bit.bit;
				sourceBits.push_back(bitIndex);
				allfilebits.push_back(bit);
			
				//this is needed to count for the #line produced by thie #include (I think)
				linenum += 1;
			
				//prioritse local path. search if it doesn't exist
				std::string relativeInclude = basefilepath(file) + include;
				if (fileExists(relativeInclude.c_str()) || includeOverrides.find(relativeInclude) != includeOverrides.end())
					include = relativeInclude;
				else
					FileFinder::find(include, include);
			
				//parse included file
				path.back() = file + ":" + intToString(linenum);
				if (!parse(include, sourceBits, defs, path))
					return false;
			
				bit = FileBit();
				bit.path = path;
				bit.filename = file;
				bit.startLine = linenum;
			}
		}
		else
		{
			//the version line (if there is one) must appear first. right after that (or before any line with content, in which case
			//the assumption is a version line is absent) the shader type defines (and later possible others) are injected.
			bool hasContent = false;
			for (int i = 0; i < (int)line.size(); ++i)
			{
				if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n')
				{
					hasContent = true;
					break;
				}
			}
			bool versionLine = (int)line.find("#version") >= 0;
			if (versionLine || (firstContentLine && hasContent))
			{
				if (versionLine)
					bit.bit += line;
				
				if (!versionLine)
				{
					firstContentLine = false;
					bit.bit += "#define " + shaderType + " 1\n";
				}
			
				//this really shouldn't be needed but line numbers are inconsistent after the #version line
				int bitIndex = allfilebits.size();
				bit.bit += "#line " + intToString(linenum+1) + " " + intToString(bitIndex) + "\n";
				
				if (!versionLine)
					bit.bit += line;
			}
			else
				bit.bit += line;
		}
		
		++linenum;
	}
	
	//add last bit
	int bitIndex = allfilebits.size();
	if (sourceBits.size() > 0) //ATI doesn't like #line before #version
		bit.bit = "#line " + intToString(bit.startLine+1) + " " + intToString(bitIndex) + "\n" + bit.bit;
	sourceBits.push_back(bitIndex);
	allfilebits.push_back(bit);
	
	FileBits justThisFile = FileBits(sourceBits.begin() + fileBitStart, sourceBits.end());
	parseCache[file] = justThisFile;
		
	return true;
}
void ShaderBuild::define(std::string def, std::string val)
{
	defines[def] = val;
}
void ShaderBuild::include(std::string filename, const char* data)
{
	includeOverrides[filename] = data;
}
bool ShaderBuild::compile(std::string file, GLenum type, Defines* defs, std::string* errStr)
{
	if (file.size() == 0)
		return true; //sure, why not

	//default defines and error string
	if (!defs) defs = &defines;
	
	//double check this file hasn't already been loaded
	if (objects.find(type) != objects.end())
		return false;
	
	//parse the file (including #includes)
	int numFileBitsBefore = allfilebits.size();
	objects[type] = ShaderSource();
	objects[type].name = file;

	if (type == GL_VERTEX_SHADER)
		shaderType = "VERTEX";
	else if (type == GL_GEOMETRY_SHADER)
		shaderType = "GEOMETRY";
	else if (type == GL_FRAGMENT_SHADER)
		shaderType = "FRAGMENT";
	else
		shaderType = "UNKNOWN_SHADER_TYPE";
	firstContentLine = true;
	
	bool success = parse(file, objects[type].source, *defs);
	
	if (!success)
	{
		//failed to parse so don't need the source anymore
		allfilebits.resize(numFileBitsBefore);
		objects.erase(objects.find(type));
		return false;
	}
	
	//print the file (debug)
	if (printProcessed)
	{
		std::cout << "/************ " << file << " ************/" << std::endl;
		for (int i = 0; i < (int)objects[type].source.size(); ++i)
		{
			int b = objects[type].source[i];
			std::stringstream filebit(allfilebits[b].bit);
			std::string line;
			int linenum = allfilebits[b].startLine;
			while (getline(filebit, line))
			{
				int s = 0;
				while((s = line.find("\t", s)) > -1) {line.replace(s, 1, "    "); s += 4;}
				std::replace(line.begin(), line.end(), '\r', '@');
				std::cout << std::setw(4) << linenum++ << "|" << line << "$" << std::endl;
			}
		}
	}
	
	//create array of shader "bits" (strings) and sizes for glShaderSource
	std::vector<const char*> strings;
	std::vector<int> sizes;
	for (int i = 0; i < (int)objects[type].source.size(); ++i)
	{
		int b = objects[type].source[i];
		strings.push_back(allfilebits[b].bit.c_str());
		sizes.push_back(allfilebits[b].bit.size());
	}
	
	//create, copy source and compile shader
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, strings.size(), &strings[0], &sizes[0]);
	glCompileShader(shader);
	
	//retrieve and parse any errors
    int logLength = 0;
    int logWritten = 0;
	int compileStatus = 0;
	std::string infoLog;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (!compileStatus)
    {
		if (logLength > 1)
		{
			infoLog.resize(logLength);
			glGetShaderInfoLog(shader, logLength, &logWritten, (char*)&infoLog[0]);
			if (errStr)
				*errStr += "=== " + file + " errors " + "===\n";
			parseLog(errStr, infoLog, objects[type].source[0], true);
		}
		
		//failed to compile. cleanup
		allfilebits.resize(numFileBitsBefore);
		objects.erase(objects.find(type));
		return false;
	}
	
	//all good, no errors.
	objects[type].object = shader;
	return true;
}
std::vector<std::string> ShaderBuild::getReferences()
{
	std::vector<std::string> ret;
	for (std::map<std::string, Lines>::iterator it = allfiles.begin(); it != allfiles.end(); ++it)
		ret.push_back(it->first);
	return ret;
}
GLuint ShaderBuild::link(std::string* errStr)
{
	//create program
	program = glCreateProgram();
	
	//attach objects
	for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
		glAttachShader(program, it->second.object);
	
	//I use these a lot. No, I don't need to reserve locations but
	//it has been helped debugging occasionally
	glBindAttribLocation(program, 0, "osVert");
	glBindAttribLocation(program, 1, "osNorm");
	glBindAttribLocation(program, 3, "osTangent");
	glBindAttribLocation(program, 4, "texCoord");
	
	//link shader objects to program
	//glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	glLinkProgram(program);
	
	//retrieve and parse any errors
    int logLength = 0;
    int logWritten = 0;
	int linkStatus = 0;
	std::string infoLog;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (!linkStatus)
    {
		if (logLength > 1)
		{
			infoLog.resize(logLength);
			glGetProgramInfoLog(program, logLength, &logWritten, (char*)&infoLog[0]);
			if (errStr)
			{
				*errStr += "=== Error linking ";
				for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
					*errStr += it->second.name + " ";
				*errStr += "===\n";
			}
			parseLog(errStr, infoLog, -1, false);
			printf("Did you call discard in a vertex shader? That pyar guy's done that before and keeps forgetting what happens.\n");
		}
		
		//failed to link
		cleanup();
		glDeleteProgram(program);
		program = 0;
	}
	else
	{
		//check the program can run
		int validateStatus = 0;
		glValidateProgram(program);
		glGetProgramiv(program, GL_VALIDATE_STATUS, &validateStatus);
		if (!validateStatus)
		{
			//failed to validate
			std::cout << "==== Error: ";
			for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
				std::cout << it->second.name << " ";
			std::cout << "failed to validate ====" << std::endl;
			cleanup();
			glDeleteProgram(program);
			program = 0;
		}
	}

	return program;
}
void ShaderBuild::cleanup()
{
	for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
		glDeleteShader(it->second.object);
	objects.clear();

	defines.clear();
	objects.clear();
	allfilebits.clear();
		
	//NOTE: it is up to the user to call glDeleteProgram when done
}
