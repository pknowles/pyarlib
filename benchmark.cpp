/* Copyright 2011 Pyarelal Knowles, under GNU GPL (see LICENCE.txt) */

#include "prec.h"

#include <assert.h>

#include <string>
#include <vector>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>
#include <map>

#include "benchmark.h"
#include "profile.h"

#include "util.h"

#include "pugixml.h"

using namespace std;

Benchmark::Value Benchmark::defaultPassthrough(0);

std::ostream& operator<<(std::ostream& os, const Benchmark::Value& v)
{
	if (floatToString(v.f) == v.s)
		os << v.f;
	else if (intToString(v.i) == v.s)
		os << v.i;
	else
		os << "\"" << v.s << "\"";
    return os;
}

Benchmark::Value::Value()
{
	i = -1;
	f = -1.0f;
	s = "invalid";
}
Benchmark::Value::Value(int ni)
{
	i = ni;
	f = (float)ni;
	s = intToString(ni);
}
Benchmark::Value::Value(float nf)
{
	i = (int)nf;
	f = nf;
	s = floatToString(nf);
}
Benchmark::Value::Value(int ni, float nf)
{
	i = ni;
	f = nf;
	s = floatToString(nf);
}
Benchmark::Value::Value(int ni, std::string ns)
{
	std::stringstream toint(ns);
	std::stringstream tofloat(ns);
	toint >> i;
	tofloat >> f;
	if (toint.fail()) i = ni;
	if (tofloat.fail()) f = (float)ni;
	s = ns;
}
Benchmark::Value::operator int() const
{
	return i;
}
Benchmark::Value::operator float() const
{
	return f;
}
Benchmark::Value::operator std::string() const
{
	return s;
}
Benchmark::Variable::Variable(std::string vname, int va, int vb, int vstep)
{
	assert(vstep != 0);
	assert((vb - va) / vstep >= 0);
	name = vname;
	a = va;
	b = vb;
	step = vstep;
	if (step == 0)
		step = (a < b ? 1 : -1);
	val = a;
	iterator = -1;
}
bool Benchmark::Variable::increment()
{
	val += step;
	if (val > b)
	{
		reset();
		return true;
	}
	return false;
}
void Benchmark::Variable::reset()
{
	val = a;
}
Benchmark::Value Benchmark::Variable::get() const
{
	if (enumVals.size() > 0)
	{
		int i = val;
		assert(i == getIndex());
		assert(i < (int)enumVals.size());
		return enumVals[i];
	}
	return Value(val);
}
int Benchmark::Variable::getIndex() const
{
	return (val - a) / step;
}
void Benchmark::Variable::setIndex(int i)
{
	val = a + i * step;
	assert(abs(val - a) <= (b - a));
}
int Benchmark::Variable::size()
{
	if (step > 0)
		return ((a<=b?1:-1) + b - a) / step;
	return 0;
}
Benchmark::Iterator::Iterator(std::vector<Variable>& nallVars) : allVars(nallVars)
{
	disable = false;
}
bool Benchmark::Iterator::increment()
{
	if (disable)
		return true;
		
	bool ok = true;
	for (size_t i = 0; i < variables.size(); ++i)
	{
		bool ended = allVars[variables[i]].increment();
		ok = ok && ended;
	}
	
	return ok;
}
void Benchmark::Iterator::reset()
{
	for (size_t i = 0; i < variables.size(); ++i)
		allVars[variables[i]].reset();
}
int Benchmark::Iterator::size()
{
	int size = allVars[variables[0]].size();
	for (size_t i = 1; i < variables.size(); ++i)
		assert(size == allVars[variables[i]].size());
	return size;
}
void Benchmark::Iterator::setIndex(int index)
{
	for (size_t i = 0; i < variables.size(); ++i)
		allVars[variables[i]].setIndex(index);
}
Benchmark::Test::Test(std::string testName, Benchmark* testOwner) : owner(testOwner), name(testName)
{
	active = false;

	match = -1;
	given = -1;
	searchStep = -1;
	
	minToStart = testOwner->defaultMinToStart;
	maxToStart = testOwner->defaultMaxToStart;
	minToTest = testOwner->defaultMinToTest;
	maxToTest = testOwner->defaultMaxToTest;
}
void Benchmark::Test::flythrough(std::string varName, int frames)
{
}
void Benchmark::Test::addConstant(std::string name, int a)
{
	addVariable(name, a, a);
}
void Benchmark::Test::addConstant(std::string name, std::string a)
{
	std::vector<std::string> l;
	l.push_back(a);
	addVariable(name, l);
}
void Benchmark::Test::addVariable(Iterator& it, std::string name, int a, int b, int step)
{
	assert(varMap.find(name) == varMap.end());
	int varID = vars.size();
	varMap[name] = varID;
	vars.push_back(Variable(name, a, b, step));
	if (vars.back().size())
	{
		vars.back().iterator = it.index;
		it.variables.push_back(varID);
	}
}
void Benchmark::Test::addVariable(Iterator& it, std::string name, std::vector<std::string> enumNames)
{
	assert(varMap.find(name) == varMap.end());
	int varID = vars.size();
	varMap[name] = varID;
	vars.push_back(Variable(name, 0, enumNames.size()-1, 1)); //NOTE: -1 because the final value is included
	//std::copy(enumNames.begin(), enumNames.end(), vars.back().enumVals.begin());
	bool allFloat = true;
	bool allInt = true;
	float testFloat;
	int testInt;
	//this is a bit terrible but I'm in a hurry here
	for (size_t i = 0; i < enumNames.size(); ++i)
	{
		allFloat = allFloat && (std::stringstream(enumNames[i]) >> testFloat);
		allInt = allInt && (std::stringstream(enumNames[i]) >> testInt) && testFloat == (float)testInt; //FIXME: this is stupid
		//std::cout << allInt << " " << testFloat << testInt
	}
	if (allInt)
	{
		for (size_t i = 0; i < enumNames.size(); ++i)
			vars.back().enumVals.push_back(Value(stringToInt(enumNames[i])));
	}
	else if (allFloat)
	{
		for (size_t i = 0; i < enumNames.size(); ++i)
			vars.back().enumVals.push_back(Value(i, stringToFloat(enumNames[i])));
	}
	else
	{
		for (size_t i = 0; i < enumNames.size(); ++i)
			vars.back().enumVals.push_back(Value(i, enumNames[i]));
	}
	if (vars.back().size())
	{
		vars.back().iterator = it.index;
		it.variables.push_back(varID);
	}
}
void Benchmark::Test::addVariable(std::string name, int a, int b, int step)
{
	if (b != a)
	{
		iterators.push_back(Iterator(vars));
		iterators.back().index = iterators.size()-1;
		addVariable(iterators.back(), name, a, b, step);
	}
	else
	{
		Iterator noiter(vars);
		addVariable(noiter, name, a, b, step);
	}
}
void Benchmark::Test::addVariable(std::string name, std::vector<std::string> enumNames)
{
	if (enumNames.size() > 1)
	{
		iterators.push_back(Iterator(vars));
		iterators.back().index = iterators.size()-1;
		addVariable(iterators.back(), name, enumNames);
	}
	else
	{
		Iterator noiter(vars);
		addVariable(noiter, name, enumNames);
	}
}
void Benchmark::Test::overrideOutput(std::string name, int val)
{
	std::stringstream v;
	v << val;
	overrides[name] = Value(val, v.str());
}
void Benchmark::Test::overrideOutput(std::string name, std::string val)
{
	overrides[name] = Value(-1, val);
}
void Benchmark::Test::matchTimes(std::string var, std::string given, int givenValA, int givenValB)
{
	//remove any previous match
	if (this->match >= 0)
		iterators[vars[this->match].iterator].disable = false;
	if (this->given >= 0)
		iterators[vars[this->given].iterator].disable = false;
	
	//start new match
	this->match = varMap[var];
	this->given = varMap[given];
	iterators[vars[this->match].iterator].disable = true;
	iterators[vars[this->given].iterator].disable = true;
	givenA = givenValA;
	givenB = givenValB;
}
Benchmark::Value Benchmark::Test::getVar(std::string name, Benchmark::Value passthrough)
{
	if (varMap.find(name) == varMap.end())
	{
		if (owner->defaults.find(name) != owner->defaults.end())
			return owner->defaults[name];
		
		std::cout << "Variable " << name << " not found." << std::endl;
		return passthrough;
	}
	int index = varMap[name];
	return vars[index].get();
}
const std::string Benchmark::Test::getVarStr(std::string name, std::string passthrough)
{
	if (varMap.find(name) == varMap.end())
	{
		if (owner->defaults.find(name) != owner->defaults.end())
			return (std::string)owner->defaults[name];
		
		std::cout << "Variable " << name << " not found." << std::endl;
		return passthrough;
	}
	return (std::string)vars[varMap[name]].get();
}
Benchmark::Value Benchmark::Test::operator[](std::string name)
{
	return getVar(name);
}
void Benchmark::Test::restart()
{
	permutations = 1;
	for (unsigned int i = 0; i < iterators.size(); ++i)
	{
		iterators[i].reset();
		if (!iterators[i].disable)
			permutations *= iterators[i].size();
	}
	
	if (match >= 0)
		searchStep = (vars[match].b - vars[match].a) / mymax(1, vars[match].step);
	searchIndex = 0;
	if (given >= 0)
	{
		vars[given].val = givenA;
		iterators[vars[given].iterator].setIndex(vars[given].getIndex());
	}
	
	maxToStart.time = mymax(minToStart.time, maxToStart.time);
	maxToStart.frames = mymax(minToStart.frames, maxToStart.frames);
	maxToTest.time = mymax(minToTest.time, maxToTest.time);
	maxToTest.frames = mymax(minToTest.frames, maxToTest.frames);
	
	totalMax.time = maxToStart.time + (maxToStart.time + maxToTest.time) * permutations;
	totalMin.time = minToStart.time + (minToStart.time + minToStart.time) * permutations;
	totalMax.frames = maxToStart.frames + (maxToStart.frames + maxToTest.frames) * permutations;
	totalMin.frames = minToStart.frames + (minToStart.frames + minToStart.frames) * permutations;
}

bool Benchmark::Test::stepTest(float lastResult)
{
	assert(active);

	if (!vars.size())
	{
		std::cout << "Warning: Test " << name << " has no variables." << std::endl;
		active = false;
		return false;
	}
	
	if (match >= 0 && given >= 0)
	{
		static float matchTimeA, matchTimeB;
		Variable& m = vars[match];
		Variable& g = vars[given];
		
		if (g.val == givenA)
		{
			//got a rendering time for givenA
			g.val = givenB;
			iterators[g.iterator].setIndex(g.getIndex());
			matchTimeA = lastResult;
			if (searchStep == 1)
				return true;
		}
		else
		{
			//got a rendering time for givenB
			g.val = givenA;
			iterators[g.iterator].setIndex(g.getIndex());
			matchTimeB = lastResult;
			
			//now we have rendering times for both, adjust the variable

			searchStep = ceil(searchStep, 2);

			static bool matchAFaster = false;
			static float lastDiff;
			float diff;
			if (matchAFaster)
				diff = matchTimeB / matchTimeA;
			else
				diff = matchTimeA / matchTimeB;
			if (searchIndex == 0)
			{
				matchAFaster = matchTimeA < matchTimeB;
				m.val += searchStep * m.step;
			}
			else
			{
				//printf("=== %f %f\n", diff, lastDiff);
				if (diff < 1.0)
				{
					m.val -= searchStep * m.step;
					//printf("Too much %s\n", m.name.c_str());
				}
				else
				{
					m.val += searchStep * m.step;
					//printf("Not enough %s\n", m.name.c_str());
				}
				m.val = myclamp(m.val, m.a, m.b);
				iterators[m.iterator].setIndex(m.getIndex());
				//printf("Step %s %i %i\n", m.name.c_str(), searchStep, m.step);
			}
			lastDiff = diff;

			++searchIndex;

			//printf("StepSize = %i, val = %i\n", searchStep, m.val);
			static int lastStep = 2;
			static int lastlastStep = 2;
			if (lastlastStep == 1 && mymin(myabs(diff-1.0f), myabs(lastDiff-1.0f)) == myabs(diff-1.0f))
			{
				//incrementVariable();
				//searchStep = (m.b - m.a) / (m.step * 2);
				searchIndex = 0;
				active = false;
				restart();
			}
			lastlastStep = lastStep;
			lastStep = searchStep;
			if (searchStep == 1)
				return true;
		}
		return false;
	}
	else
	{
		incrementVariable();
		return true;
	}
}
void Benchmark::Test::incrementVariable()
{
	unsigned int i = 0;
	//std::cout << vars[i].val << " " << vars[i].b << " " <<  vars[i].step << std::endl;
	do
	{
		if (i >= iterators.size())
		{
			active = false; //no more things to iterate. test is complete
			restart(); //last test is generally laggy, so reset variables to initial values
			//setBenchmarking(false);
			//running = false;
			break;
		}
	} while (iterators[i++].increment());
	
	if (i == 0)
		std::cout << "Warning: no iterators in test " << name << std::endl;
}
bool Benchmark::Test::complete()
{
	return !active;
}
bool Benchmark::Test::doneWarmup(float time, int frames)
{
	return minToStart.hitBoth(time, frames) || maxToStart.hitEither(time, frames);
}
bool Benchmark::Test::doneTest(float time, int frames)
{
	return minToTest.hitBoth(time, frames) || maxToTest.hitEither(time, frames);
}
std::string Benchmark::Test::getTestStr(TestStringType t)
{
	std::string sep = (t == TEST_STRING_HUMAN ? " " : ",");
	std::stringstream r;
	bool printed = false;
	for (unsigned int i = 0; i < vars.size(); ++i)
	{
		if (overrides.find(vars[i].name) != overrides.end())
			continue;
			
		if (printed)
			r << sep;
		if (t == TEST_STRING_HUMAN)
			r << vars[i].name << "=" << vars[i].get().s;
		else if (t == TEST_STRING_KEYS)
			r << "\"" + vars[i].name + "\"";
		else if (t == TEST_STRING_VALUES)
			r << vars[i].get();
		printed = true;
	}
	for (std::map<std::string, Value>::iterator it = overrides.begin(); it != overrides.end(); ++it)
	{
		if (printed)
			r << sep;
		if (t == TEST_STRING_HUMAN)
			r << it->first << "=" << it->second.s;
		else if (t == TEST_STRING_KEYS)
			r << "\"" + it->first + "\"";
		else if (t == TEST_STRING_VALUES)
			r << it->second;
		printed = true;
	}
	for (std::map<std::string, Value>::iterator it = owner->defaults.begin(); it != owner->defaults.end(); ++it)
	{
		if (varMap.find(it->first) != varMap.end())
			continue; //print all defaults, not appearing this test
		if (overrides.find(it->first) != overrides.end())
			continue;
		
		if (printed)
			r << sep;
		if (t == TEST_STRING_HUMAN)
			r << it->first << "=" << it->second.s;
		else if (t == TEST_STRING_KEYS)
			r << "\"" + it->first + "\"";
		else if (t == TEST_STRING_VALUES)
			r << it->second;
		printed = true;
	}
	return r.str();
}
std::string Benchmark::ProfileVars::getHeaders()
{
	vars.clear();
	std::string str;
	Profiler::TimeList all = profiler->getAll();
	for (size_t i = 0; i < all.size(); ++i)
	{
		vars.push_back(all[i].first);
		if (i > 0) str += ",";
		str += "\"" + all[i].first + "\"";
	}
	//printf("%s\n", str.c_str());
	return str;
}
std::string Benchmark::ProfileVars::getValues()
{
	std::stringstream str;
	for (size_t i = 0; i < vars.size(); ++i)
	{
		if (i > 0) str << ",";
		str << profiler->get(vars[i]);
	}
	//printf("%s\n", str.str().c_str());
	return str.str();
}
Benchmark::Benchmark(std::string filename)
{
	fname = filename;
	state = STATE_STOPPED;
	running = false;
	
	onChange = NULL;
	
	current = 0;
	
	ignoringNextUpdate = true;
	
	defaultMinToStart.time = 0.1f; //wait at least this long before testing
	defaultMaxToStart.time = 2.0f; //if it's really slow, just start testing
	defaultMinToStart.frames = 2; //wait for at least this many frames before testing
	defaultMaxToStart.frames = 100; //if it's really fast, just start testing
	
	defaultMinToTest.time = 0.5f; //test for at least this long, even if hit minToTest.frames already
	defaultMaxToTest.time = 2.0f; //don't bother testing any longer than this, even if minToTest.frames isn't hit yet
	defaultMinToTest.frames = 5; //test for at least this many, even if hit minToTest.time already
	defaultMaxToTest.frames = 50; //don't bother testing any more than this, even minToTest.time isn't hit yet
}
Benchmark::~Benchmark()
{
	for (size_t i = 0; i < tests.size(); ++i)
		delete tests[i];
	tests.clear();
}
bool Benchmark::isFirst()
{
	return testNum == 0;
}
void Benchmark::restart()
{
	testTotal = 0;
	for (unsigned int i = 0; i < tests.size(); ++i)
	{
		tests[i]->restart();
		tests[i]->testPermutationOffset = testTotal;
		testTotal += tests[i]->permutations;
	}
}
void Benchmark::start()
{
	if (!tests.size())
	{
		std::cout << "Benchmark has no tests. Call createTest()" << std::endl;
		return;
	}

	std::cout << "Opening " << fname << " to write results." << std::endl;
	resultsFile.open(fname.c_str());
	state = STATE_PRESTART;
	totalTime = 0;
	totalFrames = 0;
	
	for (size_t i = 0; i < profiles.size(); ++i)
		profiles[i].profiler->clearAverage(); //reset profile averaging
	
	globalTime = 0.0f;
	globalFrames = 0;
	//setBenchmarking(true);
	
	restart();
	
	current = 0;
	tests[current]->active = true;
	running = true;
	
	//notify user that the first test has started - to trigger updates that restart() has set
	if (onChange)
		onChange();
}
void Benchmark::stop()
{
	state = STATE_STOPPED;
	restart(); //last test is generally laggy, so reset variables to initial values
	running = false;
}
void Benchmark::update(float dt)
{
	if (ignoringNextUpdate)
	{
		//very useful for ignoring resource loading times between tests
		ignoringNextUpdate = false;
		return;
	}

	if (state != STATE_STOPPED)
	{
		totalFrames += 1;
		totalTime += dt;
		globalFrames += 1;
		globalTime += dt;
	}
	
	switch (state)
	{
		case STATE_PRESTART:
			if (currentTest()->doneWarmup(totalTime, totalFrames))
			{
				state = STATE_PRERUN;
				currentTest()->restart();
				//resultsFile << "#Starting tests (" << testTotal << ") ..." << std::endl;
				resultsFile << "\"" << currentTest()->name << "\"" << "," << currentTest()->getTestStr(TEST_STRING_KEYS) << ",time(ms)";
				for (size_t i = 0; i < profiles.size(); ++i)
					resultsFile << "," << profiles[i].getHeaders();
				resultsFile << std::endl;
				std::cout << std::endl;
				totalTime = 0;
				totalFrames = 0;
				testNum = 1;
			}
			break;
		case STATE_PRERUN:
			if (currentTest()->doneWarmup(totalTime, totalFrames))
			{
				state = STATE_RUNNING;
				totalTime = 0;
				totalFrames = 0;
				//std::cout << "Starting " << testNum << "/" << testTotal << std::endl;
				std::cout << currentTest()->getTestStr() << std::endl;
			}
			break;
		case STATE_RUNNING:
			if (currentTest()->doneTest(totalTime, totalFrames))
			{
				state = STATE_PRERUN;
				float timePerFrame = (totalTime * 1000.0f) / totalFrames;
				std::string testStr = currentTest()->getTestStr(TEST_STRING_VALUES); //NOTE: Needs to be before incrementTest
				
				bool recordResult = currentTest()->stepTest(timePerFrame);
				
				if (recordResult)
				{
					testNum += 1;
					//resultsFile << testStr << ": " << timePerFrame << std::endl;
					resultsFile << testNum-1 << "," << testStr << "," << timePerFrame;
					for (size_t i = 0; i < profiles.size(); ++i)
						resultsFile << "," << profiles[i].getValues();
					resultsFile << std::endl;
					resultsFile.flush();
					//std::cout << "Done " << timePerFrame << "ms" << std::endl << std::endl;
				}
				
				if (currentTest()->complete())
				{
					if (incrementTest())
					{
						std::cout << "All tests completed." << std::endl;
						state = STATE_STOPPED;
						running = false;
					}
					else
					{
						std::cout << "Beginning next test." << std::endl;
						state = STATE_PRESTART;
						for (size_t i = 0; i < profiles.size(); ++i)
							profiles[i].profiler->clearAverage(); //reset profile averaging
					}
				}
				
				//check if incrementTest has finished the last test
				if (state == STATE_STOPPED)
				{
					resultsFile.close();
				}
				
				totalTime = 0;
				totalFrames = 0;
				
				//notify user that the next test is about to begin, or benchmark has finished
				if (onChange)
					onChange();
			}
			break;
		case STATE_STOPPED:
		default:
			return;			
	}
}
Benchmark::Test* Benchmark::createTest(std::string name)
{
	Test* test = new Test(name, this);
	tests.push_back(test);
	return test;
}
bool Benchmark::incrementTest()
{
	current += 1;
	if (current >= (int)tests.size())
	{
		current = 0;
		return true;
	}
	else
		tests[current]->active = true;
	return false;
}
Benchmark::Value Benchmark::get(std::string name, Benchmark::Value passthrough)
{
	if (state == STATE_STOPPED)
	{
		if (defaults.find(name) != defaults.end())
			return defaults[name];
		return passthrough;
	}
	
	return currentTest()->getVar(name, passthrough);
}
std::string Benchmark::getStr(std::string name, std::string passthrough)
{
	if (state == STATE_STOPPED)
	{
		if (defaults.find(name) != defaults.end())
			return (std::string)defaults[name];
		return passthrough;
	}
	
	return (std::string)currentTest()->getVarStr(name, passthrough);
}
Benchmark::Value Benchmark::operator[](std::string name)
{
	return get(name);
}
int Benchmark::currentTestIndex()
{
	if (state == STATE_STOPPED)
		return 0;
	return currentTest()->testPermutationOffset + testNum;
}
int Benchmark::totalTests()
{
	return testTotal;
}
void Benchmark::callback(void (*c)(void))
{
	onChange = c;
}
Benchmark::Test* Benchmark::currentTest()
{
	if (current >= 0)
		return tests[current];
	assert(state == STATE_STOPPED);
	return NULL;
}
float Benchmark::expectedTimeToCompletion()
{
	if (state == STATE_STOPPED)
		return 0.0f;

	float frameTime = globalTime / globalFrames;
	float scaleComplete = (currentTest()->permutations - testNum) / (float)currentTest()->permutations;
	TimeAndFrame minLeft, maxLeft;
	minLeft.time = currentTest()->totalMin.time * scaleComplete;
	minLeft.frames = currentTest()->totalMin.frames * scaleComplete;
	maxLeft.time = currentTest()->totalMax.time * scaleComplete;
	maxLeft.frames = currentTest()->totalMax.frames * scaleComplete;
	for (int i = current + 1; i < (int)tests.size(); ++i)
	{
		minLeft.time += tests[i]->totalMin.time;
		minLeft.frames += tests[i]->totalMin.frames;
		maxLeft.time += tests[i]->totalMax.time;
		maxLeft.frames += tests[i]->totalMax.frames;
	}
	
	float minTime = mymax(minLeft.time, minLeft.frames * frameTime);
	float maxTime = mymin(maxLeft.time, maxLeft.frames * frameTime);
	
	return (minTime + maxTime) * 0.5f;
}
void Benchmark::setDefault(std::string name, int val)
{
	defaults[name] = Value(val);
}
void Benchmark::setDefault(std::string name, std::string val)
{
	defaults[name] = Value(-1, val);
}
void Benchmark::include(Profiler* profiler)
{
	profiles.push_back(ProfileVars());
	profiles.back().profiler = profiler;
}
void Benchmark::ignoreNextUpdate()
{
	ignoringNextUpdate = true;
}
void Benchmark::load(std::string testsFile)
{
	Benchmark::Test *test;
		
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(testsFile.c_str());
	if (!result)
	{
		cout << "Error: Could not open " << testsFile << endl;
		return;
	}
	pugi::xml_node benchmark = doc.child("benchmark");
	
	//default warmup/duration
	pugi::xml_node defaultWarmup = benchmark.child("warmup");
	if (defaultWarmup)
	{
		vector<string> time = pyarlib::split(defaultWarmup.attribute("time").value());
		vector<string> frames = pyarlib::split(defaultWarmup.attribute("frames").value());
		if (time.size())
		{
			defaultMinToStart.time = stringToFloat(time[0]);
			defaultMaxToStart.time = stringToFloat(time[1]);
		}
		if (frames.size())
		{
			defaultMinToStart.frames = stringToInt(frames[0]);
			defaultMaxToStart.frames = stringToInt(frames[1]);
		}
	}
	pugi::xml_node defaultDuration = benchmark.child("duration");
	if (defaultDuration)
	{	
		vector<string> time = pyarlib::split(defaultDuration.attribute("time").value());
		vector<string> frames = pyarlib::split(defaultDuration.attribute("frames").value());
		if (time.size())
		{
			defaultMinToTest.time = stringToFloat(time[0]);
			defaultMaxToTest.time = stringToFloat(time[1]);
		}
		if (frames.size())
		{
			defaultMinToTest.frames = stringToInt(frames[0]);
			defaultMaxToTest.frames = stringToInt(frames[1]);
		}
	}
	
	//parse all tests
	int testnum = 0;
	int createdTests = 0;
	for (pugi::xml_node testDef = benchmark.child("test"); testDef; testDef = testDef.next_sibling("test"))
	{
		++testnum;
		std::string testName = testDef.attribute("name").value();
		if (!testName.size())
			testName = "test" + intToString(testnum);
		
		std::string isDisabled = testDef.attribute("disabled").value();
		if (isDisabled.size() && stringToInt(isDisabled) != 0)
		{
			cout << "Skipping test " << testName << endl;
			continue;
		}
		
		test = createTest(testName);
		++createdTests;
		
		//read per-test warmup/duration overrides
		pugi::xml_node warmup = testDef.child("warmup");
		if (warmup)
		{
			vector<string> time = pyarlib::split(warmup.attribute("time").value());
			vector<string> frames = pyarlib::split(warmup.attribute("frames").value());
			if (time.size())
			{
				test->minToStart.time = stringToFloat(time[0]);
				test->maxToStart.time = stringToFloat(time[1]);
			}
			if (frames.size())
			{
				test->minToStart.frames = stringToInt(frames[0]);
				test->maxToStart.frames = stringToInt(frames[1]);
			}
		}
		pugi::xml_node duration = testDef.child("duration");
		if (duration)
		{	
			vector<string> time = pyarlib::split(duration.attribute("time").value());
			vector<string> frames = pyarlib::split(duration.attribute("frames").value());
			if (time.size())
			{
				test->minToTest.time = stringToFloat(time[0]);
				test->maxToTest.time = stringToFloat(time[1]);
			}
			if (frames.size())
			{
				test->minToTest.frames = stringToInt(frames[0]);
				test->maxToTest.frames = stringToInt(frames[1]);
			}
		}
		
		for (pugi::xml_node testChild = testDef.first_child(); testChild; testChild = testChild.next_sibling())
		{
			if (testChild.type() == pugi::node_pcdata)
			{
				//read all variable permutations
				stringstream vals(testChild.value());
		
				string line;
				while (getline(vals, line))
				{
					vector<string> keyval = pyarlib::map(pyarlib::trim, pyarlib::split(line, ":", 1));
					//cout << "#" << line << "$" << keyval.size() << (keyval.size() ? keyval[0].size() : 0) << endl;
					if (keyval.size() == 1 && keyval[0].size())
					{
						cout << "Warning: Invalid test key/val pair in " << line << testsFile << endl;
						continue;
					}
					else if (keyval.size() != 2)
						continue;
			
					string varname = keyval[0];
			
					//comma separated values. can be ints or strings (though ints aren't implemented yet)
					vector<string> enums = pyarlib::map(pyarlib::trim, pyarlib::split(keyval[1], ","));
					if (enums.size() > 1)
					{
						test->addVariable(varname, enums);
						continue;
					}
			
					//read iteration with format "from:to:step" just like in python
					vector<string> range = pyarlib::map(pyarlib::trim, pyarlib::split(keyval[1], ":"));
					if (range.size() == 3)
					{
						int a = stringToInt(range[0]);
						int b = stringToInt(range[1]);
						int s = stringToInt(range[2]);
						assert((b - a) / s > 0);
						test->addVariable(varname, a, b, s);
						continue;
					}
			
					int i = stringToInt(keyval[1]);
					if (keyval[1] == "-1" || i != -1)
						test->addConstant(varname, i);
					else
						test->addConstant(varname, keyval[1]);
				}
			}
		
			//read synced variables
			//for (pugi::xml_node sync = testDef.child("sync"); sync; sync = sync.next_sibling("sync"))
			if (std::string(testChild.name()) == "sync")
			{
				pugi::xml_node& sync(testChild);
				
				vector<string> range = pyarlib::split(sync.attribute("range").value(), ":");
				int rangeFrom = range.size() > 0 ? stringToInt(range[0]) : -1;
				int rangeTo = range.size() > 1 ? stringToInt(range[1]) : -1;
				int rangeInc = range.size() > 2 ? stringToInt(range[2]) : 1;
				if (rangeFrom < 0)
					rangeFrom = 0;
				rangeInc = mymax(1, rangeInc);
				
				string line;
				Iterator it(test->vars);
				it.index = test->iterators.size()-1;
				stringstream syncvals(sync.text().get());
				while (getline(syncvals, line))
				{
					//read key/value pairs
					vector<string> keyval = pyarlib::map(pyarlib::trim, pyarlib::split(line, ":", 1));
					if (keyval.size() == 1 && keyval[0].size())
					{
						cout << "Warning: Invalid test key/val pair in " << line << testsFile << endl;
						continue;
					}
					else if (keyval.size() != 2)
						continue;
				
					//parse values
					string varname = keyval[0];
					vector<string> enums = pyarlib::map(pyarlib::trim, pyarlib::split(keyval[1], ","));
					if (enums.size() > 1)
					{
						vector<string> rangedEnums;
						//cout << "range " << rangeFrom << ":" << rangeTo << ":" << rangeInc << endl;
						for (int i = rangeFrom; i < (rangeTo >= 0 ? rangeTo : (int)enums.size()); i += rangeInc)
						{
							//cout << i << " ";
							rangedEnums.push_back(enums[i]);
						}
						//cout << endl;
					
						//cout << varname << ":" << pyarlib::join("#", enums) << endl;
						test->addVariable(it, varname, rangedEnums);
						continue;
					}
				}
			
				//cout << "Created sync for " << it.variables.size() << " variables." << endl;
				if (it.variables.size())
					test->iterators.push_back(it);
			}
		}
	}
	
	for (pugi::xml_node benchmarkChild = benchmark.first_child(); benchmarkChild; benchmarkChild = benchmarkChild.next_sibling())
	{
		if (benchmarkChild.type() == pugi::node_pcdata)
		{
			//parse default values
			stringstream vals(benchmarkChild.value());
			string line;
			while (getline(vals, line))
			{
				vector<string> keyval = pyarlib::map(pyarlib::trim, pyarlib::split(line, ":", 1));
				if (keyval.size() == 1 && keyval[0].size())
				{
					cout << "Warning: Invalid test key/val pair in " << testsFile << endl;
					continue;
				}
				else if (keyval.size() != 2)
					continue;
		
				string varname = keyval[0];
		
				int i = stringToInt(keyval[1]);
				if (keyval[1] == "-1" || i != -1)
					setDefault(varname, i);
				else
					setDefault(varname, keyval[1]);
			}
		}
	}
	
	if (createdTests == 0)
		cout << "Warning: Test tile " << testsFile << " has no tests." << endl;
}
