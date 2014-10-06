#pragma once
#ifndef KD_TREE
#define KD_TREE

#include <stdio.h>

class KDTreeIterator;

class KDTree
{
	friend class KDTreeIterator;
private:
	struct Node	{float pos; bool leaf; int a; int b;};
	Node* nodes; //Ahnentafel list
	int k;
	int n;
	int maxdepth;
	int targetLeafSize;
	int numNodes;
	float* points;
	int* idlist;
	int curid;
	int count;
	void build(int a, int b, int depth);
	KDTree(const KDTree& other) {printf("SHOULDNT SEE THIS\n");}; //can't copy tree
	void operator=(const KDTree& other) {printf("SHOULDNT SEE THIS\n");}; //can't assign tree
public:
	KDTree(int k, int targetLeafSize = 10);
	~KDTree();
	void setPoints(float* points, int count);
	void rebuild();
	KDTreeIterator find(float* position, float radius);
	void debugLines(float** data, int* lines); //data = vertex coord + colour
};

class KDTreeIterator
{
	friend class KDTree;
private:
	float* pos;
	float radius;
	int curdepth;
	int cur;
	int leafindex;
	bool* tovisit;
	int* instances;
	KDTree& tree;
	KDTreeIterator(KDTree& kdtree, float* pos, float radius);
	void operator=(const KDTreeIterator& other) {}; //can't assign iterator
public:
	KDTreeIterator(const KDTreeIterator& other);
	~KDTreeIterator();
	int next();
};

#endif
