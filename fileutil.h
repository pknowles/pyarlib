/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef FILEUTIL_H
#define FILEUTIL_H

std::string expanduser(std::string filename);
std::string basefilename(std::string filename);
std::string basefilepath(std::string filename);
std::string fileExtension(std::string filename);
std::string joinPath(const std::string& a, const std::string& b);
std::vector<std::string> listDirectory(std::string path);
bool fileExists(const char* filename);
int fileTime(const char* filename);
bool pathExists(const char* path);
bool readFile(std::string& str, const char* filename);
bool readUncomment(std::istream& stream, std::string& line); //not thread safe!

#endif
