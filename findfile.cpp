 
#include "prec.h"
#include "findfile.h"
#include "fileutil.h"
#include "config.h"

FileFinder* FileFinder::instance = NULL;
FileFinder* FileFinder::getSingleton()
{
	if (!instance)
	{
		instance = new FileFinder();
		instance->addDir(Config::getString("root"));
	}
	return instance;
}
FileFinder::FileFinder()
{
}
FileFinder::~FileFinder()
{
}
bool FileFinder::find(const std::string& name, std::string& found)
{
	if (name.size())
	{
		//take local path/cwd as priority
		if (fileExists(name.c_str()))
		{
			found = name;
			return true;
		}
		
		//try all search paths
		for (std::set<std::string>::iterator it = getSingleton()->paths.begin(); it != getSingleton()->paths.end(); ++it)
		{
			std::string test = joinPath(*it, name);
			//printf("looking for %s in %s\n", name.c_str(), test.c_str());
			if (fileExists(test.c_str()))
			{
				//printf("FOUND\n");
				found = test;
				return true;
			}
		}
	}
	return false;
}
std::string FileFinder::find(const std::string& name)
{
	std::string result;
	find(name, result);
	return result;
}
bool FileFinder::addDir(std::string path, bool recursive)
{
	path = expanduser(path);
	
	//try appending the apps root directory if the relative path doesn't exist
	if (!pathExists(path.c_str()))
	{
		//printf("%s doesn't exist. ", path.c_str());
		path = joinPath(Config::getString("root"), path);
		//printf("trying %s.\n", path.c_str());
	}

	if (!pathExists(path.c_str()))
	{
		printf("NOTE: Path %s doesn't exist. Not adding to FileFinder\n", path.c_str());
		return false;
	}
	
	getSingleton()->paths.insert(path);
	if (recursive)
		printf("Warning: FileFinder::addDir(path, true) not implemented\n");
	return true;
}
