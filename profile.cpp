/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include "includegl.h"
#include "profile.h"
#include "shader.h"
#include "util.h"

#include <assert.h>

Profiler::Query::Query()
{
	timeFrom = -1;
	timeTo = -1;
	multipleTimes = false;
}
Profiler::Profiler()
{
	ready = false;
	current = 0;
}
void Profiler::clear()
{
	//FIXME: could free and clear queryObjs too
	ready = false;
	queries.clear();
	queryOrder.clear();
	lastQueryOrder.clear();
}
void Profiler::clearAverage()
{
	for (QueryMap::iterator it = queries.begin(); it != queries.end(); ++it)
		it->second.times.clear();
}
void Profiler::restartQueries()
{
	current = 0;
	lastQueryOrder.clear();
	std::swap(lastQueryOrder, queryOrder);
}
GLuint Profiler::getNextQuery()
{
	if (current > 50)
	{
		printf("Error: too many queries. Did you forget to call Profiler::begin()?\n");
		return 0;
	}

	//allocate another query if needed
	GLuint query;
	if ((int)queryObjs.size() < current + 1)
	{
		glGenQueries(1, &query);
		queryObjs.push_back(query);
	}
	else
		query = queryObjs[current];
	current++;
	return query;
}
void Profiler::begin()
{
	if (current > 0 && nameCount.size() == 0)
		time("Total"); //default, time everything if only begin() is called. required for glGetQueryObjectiv
	
	//printf("begin\n");
	nameCount.clear();
	
	assert(!CHECKERROR);
	
	if (ready)
	{
		ready = false;
	}

	if (queries.size())
	{
		//if the last query is done, they are all done
		GLint available;
		glGetQueryObjectiv(queryObjs[current-1], GL_QUERY_RESULT_AVAILABLE, &available);
		if (available)
		{
			//get the results
			timeStamps.resize(queryObjs.size());
			for (int i = 0; i < (int)queryObjs.size(); ++i)
			{
				CHECKERROR;
				GLuint64 result;
				glGetQueryObjectui64vEXT(queryObjs[i], GL_QUERY_RESULT, &result);
				timeStamps[i] = result;
				CHECKERROR;
			}
			
			//append the differences to the samples list
			for (QueryMap::iterator it = queries.begin(); it != queries.end(); ++it)
			{
				//if the program suddenly stops timing something don't remove the query, just ignore it
				if (it->second.timeTo == -1 && it->second.timeFrom == -1)
					continue;
				
				if (it->second.timeTo == -1)
				{
					printf("Warning: no ending time() for start(%s) in profiler\n", it->first.c_str());
					continue;
				}
				
				if (it->second.timeFrom < 0 || it->second.timeTo < 1)
					printf("Error: invalid timestamp indices %i -> %i\n", it->second.timeFrom, it->second.timeTo);
				
				//get the time difference
				assert(it->second.timeFrom < it->second.timeTo);
				GLuint64 timediff = timeStamps[it->second.timeTo] - timeStamps[it->second.timeFrom];
				
				if (it->second.times.size() && myabs(((GLint64)it->second.times.back() - (GLint64)timediff)) > it->second.times.back() * 0.5)
					it->second.times.clear(); //clear samples if the new one is different by more than 50%
					
				it->second.times.push_back(timediff);
				if (it->second.times.size() > PROFILE_SAMPLES)
					it->second.times.pop_front();
					
				it->second.multipleTimes = (it->second.timeFrom != it->second.timeTo - 1);
				it->second.timeFrom = -1;
				it->second.timeTo = -1;
			}
			
			//start the next set of samples
			ready = true;
			restartQueries();
		}
	}
	else
		ready = true;
	
	if (ready)
	{
		glQueryCounter(getNextQuery(), GL_TIMESTAMP);
	}
}
void Profiler::start(std::string name)
{
	//printf("start %s\n", name.c_str());
	
	assert(!CHECKERROR);
	if (ready)
	{
		//create query struct for "name" if there isn't one
		QueryMap::iterator q = queries.find(name);
		if (q == queries.end())
		{
			std::pair<QueryMap::iterator, bool> ret;
			ret = queries.insert(make_pair(name, Query()));
			q = ret.first;
		}
		
		//if "name" has already been set after begin(), ignore this call
		if (q->second.timeTo > 0)
		{
			printf("Warning: attempting to set Profiler::start after the end time\n");
			return;
		}
		
		//set the start time to the current query
		if (q->second.timeFrom < 0)
			q->second.timeFrom = current - 1;
	}
}
void Profiler::time(std::string name)
{
	//printf("time %s\n", name.c_str());
	
	assert(!CHECKERROR);
	
	//NOTE: for future Pyar... if you aren't getting time() results, you're calling begin() twice.
	//this is the SECOND time you've had this issue, learn to remember!
	
	//generate a unique name for this occurance after begin()
	if (nameCount.find(name) != nameCount.end())
	{
		nameCount[name] += 1;
		name += intToString(nameCount[name]);
	}
	else
		nameCount[name] = 1;
	
	if (ready)
	{
		//create query struct for "name" if there isn't one
		QueryMap::iterator q = queries.find(name);
		if (q == queries.end())
		{
			std::pair<QueryMap::iterator, bool> ret;
			ret = queries.insert(make_pair(name, Query()));
			q = ret.first;
		}
		
		//if "name" has already been set after begin(), ignore this call
		if (q->second.timeTo > 0)
			return;
		
		//attach this timestamp to "name"
		if (q->second.timeFrom < 0)
			q->second.timeFrom = current - 1; //default difference to previous time() call
		assert(current > 0);
		q->second.timeTo = current;
		queryOrder.push_back(q->first);
		
		//move to the next query
		CHECKERROR;
		glQueryCounter(getNextQuery(), GL_TIMESTAMP);
		//NOTE: if this causes an error it's probably because you're using AMD
		//my guess is AMD won't allow two querys at the same time even of differing types
		CHECKERROR;
	}
}
float Profiler::get(std::string name)
{
	QueryMap::iterator q = queries.find(name);
	if (q == queries.end() || q->second.times.size() == 0)
		return -1.0f;
	
	Times& t = q->second.times;
	GLuint64 result = 0;
	for (Times::iterator it = t.begin(); it != t.end(); ++it)
		result += *it;
	return (float)((double)result / (t.size() * 1000000));
}

std::vector<std::pair<std::string, float> > Profiler::getAll()
{
	std::vector<std::pair<std::string, float> > ret;
	ret.reserve(queries.size());
	//for (QueryMap::iterator it = queries.begin(); it != queries.end(); ++it)
	for (int i = 0; i < (int)lastQueryOrder.size(); ++i)
	{
		QueryMap::iterator it = queries.find(lastQueryOrder[i]);
		//ret.push_back(make_pair((it->second.multipleTimes ? "*" : "") + it->first, get(it->first)));
		ret.push_back(make_pair(it->first, get(it->first)));
	}
	return ret;
}
std::string Profiler::toString()
{
	std::stringstream s;
	s << std::fixed << std::setprecision(2);
	for (int i = 0; i < (int)lastQueryOrder.size(); ++i)
	{
		QueryMap::iterator it = queries.find(lastQueryOrder[i]);
		if (it != queries.end())
			s << get(it->first) << (it->second.multipleTimes ? "ms *" : "ms ") << it->first << "\n";
	}
	return s.str();
}
