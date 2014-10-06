/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef PROFILE_H
#define PROFILE_H

#define PROFILE_SAMPLES 20

#include <list>
#include <vector>
#include <map>
#include <string>

#include "includegl.h"

//If you don't see any variables appearing, it's probably because you haven't called begin() or are calling it multiple times a frame

class Profiler
{
	bool ready;
	typedef std::list<GLuint64> Times;
	struct Query
	{
		int timeFrom;
		int timeTo;
		bool multipleTimes;
		Times times;
		Query();
	};
	typedef std::map<std::string, Query> QueryMap;
	std::vector<GLuint> queryObjs;
	std::vector<GLuint64> timeStamps;
	std::vector<std::string> queryOrder; //for toString
	std::vector<std::string> lastQueryOrder; //for toString
	std::map<std::string, int> nameCount; //to resolve multiple calls with the same name
	QueryMap queries;
	
	int current;
	void restartQueries();
	GLuint getNextQuery();
public:
	typedef std::pair<std::string, float> Time;
	typedef std::vector<Time> TimeList;
	Profiler();
	void clear(); //removes all queries from name map
	void clearAverage(); //remove current averaged time. generally call when starting a new benchmark test
	void begin(); //call at the start of each frame
	void start(std::string name); //manually set the start time - default assumes time from previous time() call.
	void time(std::string name); //milliseconds
	float get(std::string name);
	TimeList getAll();
	std::string toString();
};

#endif
