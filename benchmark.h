/* Copyright 2011 Pyarelal Knowles, under GNU GPL (see LICENCE.txt) */


#ifndef BENCHMARK_H
#define BENCHMARK_H

class Profiler;

class Benchmark
{
public:
	struct Value
	{
		int i;
		float f;
		std::string s;
		Value();
		Value(int ni);
		Value(float nf);
		Value(int ni, float nf); //optional provide int enumerator for float
		Value(int ni, std::string ns); //if string, provide index for fallback
		explicit operator int() const;
		explicit operator float() const;
		explicit operator std::string() const;
		friend std::ostream& operator<<(std::ostream& os, const Value& v);
	};
private:
	struct Variable
	{
		friend class Benchmark;
		std::string name;
		std::vector<Value> enumVals; //use to create variable/name association
		int a, b;
		int step;
		int val;
		int iterator;
		Variable(std::string vname, int va, int vb, int vstep);
		bool increment();
		void reset();
		Value get() const;
		int getIndex() const;
		void setIndex(int i);
		int size();
	};
	struct Iterator
	{
		std::vector<Variable>& allVars;
		std::vector<int> variables;
		bool disable; //for use in time matching
		Iterator(std::vector<Variable>& nallVars);
		bool increment();
		void reset();
		void setIndex(int index);
		int size();
		int index;
	};
	enum State
	{
		STATE_PRESTART,
		STATE_PRERUN,
		STATE_RUNNING,
		STATE_STOPPED
	};
public:
	struct ProfileVars
	{
		Profiler* profiler;
		std::vector<std::string> vars;
		std::string getHeaders();
		std::string getValues(); //only includes names existing when getHeaders() was called
	};
	struct TimeAndFrame {
		float time;
		int frames;
		bool hitEither(float t, int n) {return t >= time || n >= frames;}
		bool hitBoth(float t, int n) {return t >= time && n >= frames;}
	};
	enum TestStringType
	{
		TEST_STRING_HUMAN,
		TEST_STRING_KEYS,
		TEST_STRING_VALUES,
	};
	class Test {
		bool active;
		
		friend class Benchmark;
		Benchmark* owner;
		
		std::vector<Variable> vars;
		std::vector<Iterator> iterators; //used to group and iterate multiple variables at once
		std::map<std::string, int> varMap;
		std::map<std::string, Value> overrides;
		
		int permutations;
		int testPermutationOffset;
		TimeAndFrame totalMin, totalMax;

		//vars for matching times. see matchTimes()
		int match;
		int given;
		int searchStep;
		int searchIndex;
		int givenA;
		int givenB;
		
		Test(std::string testName, Benchmark* testOwner);
		void addVariable(Iterator& it, std::string name, int a, int b, int step = 1);
		void addVariable(Iterator& it, std::string name, std::vector<std::string> enumNames);
	public:
		std::string name;
		TimeAndFrame minToStart;
		TimeAndFrame maxToStart;
		TimeAndFrame minToTest;
		TimeAndFrame maxToTest;
		void flythrough(std::string varName, int frames); //initializes this set for benchmarking a flythrough. removes ALL other variables!
		void addConstant(std::string name, int a);
		void addConstant(std::string name, std::string a);
		void addVariable(std::string name, int a, int b, int step = 1);
		void addVariable(std::string name, std::vector<std::string> enumNames);
		void overrideOutput(std::string name, int val);
		void overrideOutput(std::string name, std::string val);
		
		//performs a binary search to find variables producing similar frame times.
		//by varying "var", find similar times for "given" = "givenValA" and "givenValB".
		//For example, use "fragments" to find a similar rendering time for "algorithm" 3 and 4
		void matchTimes(std::string var, std::string given, int givenValA, int givenValB);
		
		Value getVar(std::string name, Value passthrough = 0);
		const std::string getVarStr(std::string name, std::string passthrough = "");
		Value operator[](std::string name); //same as getVar(), but with default passthrough
		void restart();
		bool stepTest(float lastResult);
		void incrementVariable();
		bool complete();
		bool doneWarmup(float time, int frames);
		bool doneTest(float time, int frames);
		std::string getTestStr(TestStringType t = TEST_STRING_HUMAN);
	};
private:
	
	State state;
	float totalTime;
	int current;
	int totalFrames;
	float globalTime;
	int globalFrames;
	int testNum;
	int testTotal;
	std::string fname;
	std::ofstream resultsFile;
	
	std::vector<Test*> tests;
	std::map<std::string, Value> defaults;
	std::vector<ProfileVars> profiles;
	
	void (*onChange)(void);
	
	bool ignoringNextUpdate;

	//no copying
	Benchmark(const Benchmark& other) {}
	void operator=(const Benchmark& other) {}
	bool incrementTest(); //returns true if finished
	void restart();
public:
	TimeAndFrame defaultMinToStart;
	TimeAndFrame defaultMaxToStart;
	TimeAndFrame defaultMinToTest;
	TimeAndFrame defaultMaxToTest;
	
	static Value defaultPassthrough;
	
	Benchmark(std::string filename = "benchmark.csv");
	virtual ~Benchmark();
	bool running;
	bool isFirst();
	void start();
	void stop(); //stop tests before completion
	void update(float dt);
	Test* createTest(std::string name);
	Value get(std::string name, Value passthrough = defaultPassthrough);
	std::string getStr(std::string name, std::string passthrough = "");
	Value operator[](std::string name); //same as get(), but with default passthrough
	int currentTestIndex();
	int totalTests();
	void callback(void (*c)(void));
	Test* currentTest();
	float expectedTimeToCompletion();
	void setDefault(std::string name, int val);
	void setDefault(std::string name, std::string val);
	void include(Profiler* profiler);
	void ignoreNextUpdate();
	void load(std::string testsFile);
};

std::ostream& operator<<(std::ostream& os, const Benchmark::Value& v);

#endif

