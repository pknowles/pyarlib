
#ifndef FINDFILE_H
#define FINDFILE_H

class FileFinder
{
private:
	std::set<std::string> paths;
	static FileFinder* instance;
	static FileFinder* getSingleton();
	FileFinder();
	~FileFinder();
public:
	static bool find(const std::string& name, std::string& found);
	static std::string find(const std::string& name);
	static bool addDir(std::string path, bool recursive = false);
};

#endif
