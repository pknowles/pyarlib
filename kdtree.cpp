
#include "prec.h"
#include "util.h"
#include "kdtree.h"
#include <string.h>
#include <assert.h>
#include <math.h>
#include <algorithm>
#include <vector>
using namespace std;

static float* sortPoints;
static int axis;
static int sortK;
static const bool comparePoints(const int a, const int b)
{
	return sortPoints[a*sortK+axis] < sortPoints[b*sortK+axis];
}

KDTree::KDTree(int k, int targetLeafSize)
{
	this->k = k;
	this->targetLeafSize = targetLeafSize;
	nodes = NULL;
	idlist = NULL;
	n = 0;
	count = 0;
}
KDTree::~KDTree()
{
	delete[] nodes;
	delete[] idlist;
}
void KDTree::build(int a, int b, int depth)
{
	//FIXME: very small numbers of points (maybe all at the same position) can cause a crash
	assert(n < numNodes);
	nodes[n].a = a;
	nodes[n].b = b;
	
	//stop if reached maximum depth or target leaf size
	if (depth >= maxdepth || count <= targetLeafSize)
		nodes[n].leaf = true; //set node as leaf and add points
	else
	{
		//set node as leaf and split points along approx median
		axis = depth % k;
		nodes[n].leaf = false;
		
		//find true median
		sort(idlist + a, idlist + b, comparePoints);
		int median = a + (b-a)/2;
		nodes[n].pos = points[idlist[median]*k+axis];
		if (median > 1 && median % 2 == 0) nodes[n].pos = (nodes[n].pos + points[idlist[median+1]*k+axis]) * 0.5f;
		
		//build left node
		int cur = n;
		n = cur*2+1;
		build(a, median, depth + 1);
		
		//build right node
		n = cur*2+2;
		build(median, b, depth + 1);
	}
}
void KDTree::setPoints(float* points, int count)
{
	if (this->count != count)
	{
		//free old tree memory (if there is any)
		delete[] nodes;
		delete[] idlist;

		//calculate a good maximum depth based on target leaf size
		maxdepth = mymax(0, 1+(int)log2((float)count / targetLeafSize));

		//find number of nodes for Ahnentafel list
		for (int i = 0; i <= maxdepth+1; ++i) //enough for tree and leaves
			numNodes += 1 << i;

		//allocate tree memory
		nodes = new Node[numNodes];
		idlist = new int[count];
		
		//printf("kdtree maxdepth: %i\n", maxdepth);
		

		for (int i = 0; i < count; ++i)
			idlist[i] = i;
	}
	
	this->points = points;
	this->count = count;
}
void KDTree::rebuild()
{
	if (maxdepth > 0 && count > targetLeafSize)
	{
		curid = 0;
		n = 0;
		sortPoints = points;
		sortK = k;
		build(0, count, 0);
	}
}
KDTreeIterator KDTree::find(float* position, float radius)
{
	return KDTreeIterator(*this, position, radius);
}
void KDTree::debugLines(float** data, int* lines)
{
	static float* d = NULL;
	static int c = 0;
	if (c != n)
	{
		c = n;
		delete[] d;
		d = new float[c*(k+3)*2];
	}
	*data = d;
	float* l = d;
	*lines = 0;
	float* mn = new float[k];
	float* mx = new float[k];
	int* block = new int[k];
	for (int i = 0; i < n; ++i)
	{
		if (nodes[i].leaf) continue;
		int depth;
		depth = (int)log2((float)i+1);
		int axis = depth%k;
		float pos = nodes[i].pos;
		
		for (int a = 0; a < k; ++a)
		{
			mn[a] = -20.0;
			mx[a] =  20.0;
		}
		
		block[0] = i;
		while (block[0] > 0)
		{
			for (int a = 1; a < k; ++a)
			{
				block[a] = (block[a-1]-1)/2;
				if (block[a-1] == 0) break;
				if (block[a-1]%2==1) mx[a] = min(mx[a], nodes[block[a]].pos);
				if (block[a-1]%2==0) mn[a] = max(mn[a], nodes[block[a]].pos);
			}
			if (k > 1 && block[k-2] == 0) break;
			block[0] = (block[k-1]-1)/2;
		}
		for (int a = 0; a < k; ++a)
			l[(k-a+axis)%k] = mn[a];
		l[axis] = pos;
		l[k+0] = (float)(axis%3 == 0)/(depth*0.1+1);
		l[k+1] = (float)(axis%3 == 1)/(depth*0.1+1);
		l[k+2] = (float)(axis%3 == 2)/(depth*0.1+1);

		l += k+3;
		for (int a = 0; a < k; ++a)
			l[(k-a+axis)%k] = mx[a];
		l[axis] = pos;
		l[k+0] = (float)(axis%3 == 0)/(depth*0.1+1);
		l[k+1] = (float)(axis%3 == 1)/(depth*0.1+1);
		l[k+2] = (float)(axis%3 == 2)/(depth*0.1+1);
		l += k+3;
		*lines += 1;
	}
	delete[] mn;
	delete[] mx;
	delete[] block;
}
KDTreeIterator::KDTreeIterator(KDTree& kdtree, float* pos, float radius) : tree(kdtree)
{
	this->pos = new float[tree.k];
	memcpy(this->pos, pos, sizeof(float) * tree.k);
	this->radius = radius;
	instances = new int(0);
	curdepth = 0;
	tovisit = new bool[tree.maxdepth];
	cur = 0;
	leafindex = 0;
	
	if (tree.numNodes <= 0)
		curdepth = -1;
}
KDTreeIterator::KDTreeIterator(const KDTreeIterator& other) : tree(other.tree)
{
	memcpy(this, &other, sizeof(KDTreeIterator));
	*instances += 1;
}
KDTreeIterator::~KDTreeIterator()
{
	delete instances;
	delete[] pos;
	delete[] tovisit;
}
int KDTreeIterator::next()
{
	if (curdepth < 0)
		return -1; //traversal has finished... still
	if (tree.nodes[cur].leaf && tree.nodes[cur].a + leafindex >= tree.nodes[cur].b)
	{
		do {cur = (cur-1)/2;} while (tovisit[--curdepth] == false && curdepth >= 0);
		if (curdepth >= 0) {tovisit[curdepth] = false; cur = cur*2+2; ++curdepth;} //travers right node
		else return -1; //traversal has finished
		leafindex = 0;
	}
	while (!tree.nodes[cur].leaf)
	{
		if (tree.nodes[cur].pos > pos[curdepth % tree.k] - radius)
		{
			if (tree.nodes[cur].pos < pos[curdepth % tree.k] + radius)
				tovisit[curdepth++] = true; //mark right branch to be traversed later
			else
				tovisit[curdepth++] = false; //only need to check the left side
			cur = cur*2+1;
		}
		else
		{
			cur = cur*2+2;
			tovisit[curdepth++] = false; //only need to check the right side
		}
		leafindex = 0;
	}
	return tree.idlist[tree.nodes[cur].a + (leafindex++)]; //return ids in current leaf node
}
