/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <string>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

std::string getHomeDir()
{
	static std::string home;
#ifndef _WIN32
	if (!home.size())
		home = getenv("HOME");
	if (!home.size())
		home = getenv("USERPROFILE");
	if (!home.size())
	{
		char* drive = getenv("HOMEDRIVE");
		char* path = getenv("HOMEPATH");
		home = std::string(drive) + path;
	}
#endif
	return home;
}

std::string expanduser(std::string filename)
{
	if (filename[0] == '~' && (filename.size() == 0 || filename[1] == '/'))
	{
		filename.replace(0, 1, getHomeDir());
	}
	return filename;
}
std::string basefilename(std::string filename)
{
	size_t s = filename.find_last_of("/");
	size_t p = filename.find_last_of(".");
	s = (int)s < 0 ? 0 : s + 1;
	if ((int)p == -1 || p < s)
		p = filename.size();
	return filename.substr(s, p-s);
}
std::string basefilepath(std::string filename)
{
	size_t p = filename.find_last_of("/");
	if ((int)p >= 0)
		return filename.substr(0, p + 1);
	return "";
}
std::string fileExtension(std::string filename)
{
	int i = filename.find_last_of(".");
	if (i > 0)
		return filename.substr(i+1);
	return "";
}
std::string joinPath(const std::string& a, const std::string& b)
{
	if (!a.size())
		return b;
	
	if (a[a.size()-1] == '/')
		return a + b;
	return a + "/" + b;
}
std::vector<std::string> listDirectory(std::string path)
{
	std::vector<std::string> ret;
#ifdef _WIN32
	printf("Error: no listDirectory() implemented for windows in pyarlib\n");
#else
	DIR *dp;
	struct dirent *ep;     
	dp = opendir(path.c_str());
	if (dp != NULL)
	{
		while ((ep = readdir(dp)))
			ret.push_back(ep->d_name);
		closedir(dp);
	}
#endif
	return ret;
}
bool fileExists(const char* filename)
{ 
#ifdef _WIN32
	DWORD fileAttr;
	fileAttr = GetFileAttributesA(filename);
	if (0xFFFFFFFF == fileAttr)
		return false;
	return true;
#else
	struct stat info;
	return stat(filename, &info) == 0;
#endif
}
bool pathExists(const char* path)
{
	//FIXME: untested on windows
	return fileExists(path);
}
int fileTime(const char* filename)
{
#ifdef _WIN32
	//is it just me or is it normal to feel like throwin up when
	//ever I have to deal with the windows API
	FILETIME modtime;
	HANDLE fh = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE)
		return 0;
	if (GetFileTime(fh, NULL, NULL, &modtime) == 0)
		return 0;
	CloseHandle(fh);
	int tmp = (((__int64)modtime.dwHighDateTime) << 32)/10000000;
	return tmp + modtime.dwLowDateTime/10000000; //NOTE: untested :D
#else
	struct stat info;
	if (stat(filename, &info) == 0)
		return info.st_mtime;
	return 0;
#endif
}
bool readFile(std::string& str, const char* filename)
{
	if (!filename) return false;
	std::ifstream ifile(filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (!ifile.good()) return false;
	str.reserve((unsigned int)ifile.tellg()+1); //reserve file size
	ifile.seekg(0); //go to start
	
	str.assign((std::istreambuf_iterator<char>(ifile)), std::istreambuf_iterator<char>());
	//ifile.read((char*)&str[0], str.size());
	
	ifile.close();
	return true;
}

bool readUncomment(std::istream& stream, std::string& line)
{
	//FIXME: does not handle "strings with //comments" or 'literals/*such as this'
	static bool commentBlock = false;
	if (!getline(stream, line))
		return false;
	
	//if currently within a block/c-style comment
	if (commentBlock)
	{
		int bcomment = line.find("*/");
		if (bcomment < 0)
		{
			line = "\n";
			return true;
		}
		else
			line = line.substr(bcomment+2);
		commentBlock = false;
	}
		
	//remove single line block/c-style comments
	while (true)
	{
		int first = line.find("/*");
		if (first < 0) break;
		int second = line.find("*/", first);
		if (second < 0) break;
		line = line.substr(0, first) + line.substr(second+2);
	}

	//remove "//" comments
	int lcomment = line.find("//");
	if (lcomment >= 0)
	{
		line = line.substr(0, lcomment);
	}
	
	//check for multiline block comments
	int bcomment = line.find("/*");
	if (bcomment >= 0)
	{
		line = line.substr(0, bcomment);
		commentBlock = true;
	}
	
	line += "\n";
	
	bool done = !stream.good();
	if (done) commentBlock = false;
	return true;
}

std::string stripComments(const std::string& text)
{
	std::stringstream str(text), out;
	std::string line;
	while (readUncomment(str, line))
		out << line;
	return out.str();
}

