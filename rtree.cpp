
#include "prec.h"

#include "includegl.h"
#include "util.h"

#include "rtree.h"

#include <algorithm>
#include <set>

#include <assert.h>

void RTree::Box::draw()
{
	glBegin(GL_LINES);
	glVertex3f(bmin.x, bmin.y, bmin.z); glVertex3f(bmin.x, bmin.y, bmax.z);
	glVertex3f(bmin.x, bmin.y, bmax.z); glVertex3f(bmin.x, bmax.y, bmax.z);
	glVertex3f(bmin.x, bmax.y, bmax.z); glVertex3f(bmin.x, bmax.y, bmin.z);
	glVertex3f(bmin.x, bmax.y, bmin.z); glVertex3f(bmin.x, bmin.y, bmin.z);
	
	glVertex3f(bmax.x, bmin.y, bmin.z); glVertex3f(bmax.x, bmin.y, bmax.z);
	glVertex3f(bmax.x, bmin.y, bmax.z); glVertex3f(bmax.x, bmax.y, bmax.z);
	glVertex3f(bmax.x, bmax.y, bmax.z); glVertex3f(bmax.x, bmax.y, bmin.z);
	glVertex3f(bmax.x, bmax.y, bmin.z); glVertex3f(bmax.x, bmin.y, bmin.z);
	
	glVertex3f(bmin.x, bmin.y, bmin.z); glVertex3f(bmax.x, bmin.y, bmin.z);
	glVertex3f(bmin.x, bmin.y, bmax.z); glVertex3f(bmax.x, bmin.y, bmax.z);
	glVertex3f(bmin.x, bmax.y, bmax.z); glVertex3f(bmax.x, bmax.y, bmax.z);
	glVertex3f(bmin.x, bmax.y, bmin.z); glVertex3f(bmax.x, bmax.y, bmin.z);
	glEnd();
}

RTree::Box::Box()
{
}

RTree::Box::Box(const vec3f& nbmin, const vec3f& nbmax) : bmin(nbmin), bmax(nbmax)
{
}

float RTree::Box::volume() const
{
	vec3f c = bmax - bmin;
	float r = c.x * c.y * c.z;
	return r;
}

float RTree::Box::edgeLength() const
{
	//FIXME: should margin be total edge length or surface area in the case of a volume
	vec3f c = bmax - bmin;
	return (c.x + c.y + c.z) * 2.0f;
}

float RTree::Box::surfaceArea() const
{
	//FIXME: should margin be total edge length or surface area in the case of a volume
	vec3f c = bmax - bmin;
	return (c.x * c.y + c.x * c.z + c.y * c.z) * 2.0f;
}
		
float RTree::Box::funion(const Box& box) const
{
	vec3f a = vmin(bmin, box.bmin);
	vec3f b = vmax(bmax, box.bmax);
	vec3f c = b - a;
	return c.x * c.y * c.z;
}
float RTree::Box::fintersect(const Box& box) const
{
	if (!intersects(box))
		return 0.0f;
	vec3f a = vmax(bmin, box.bmin);
	vec3f b = vmin(bmax, box.bmax);
	vec3f c = b - a;
	return c.x * c.y * c.z;
}
RTree::Box RTree::Box::bunion(const Box& box) const
{
	return Box(vmin(bmin, box.bmin), vmax(bmax, box.bmax));
}
bool RTree::Box::intersects(const Box& box) const
{
	return bmin.x < box.bmax.x &&
		bmin.y < box.bmax.y &&
		bmin.z < box.bmax.z &&
		bmax.x > box.bmin.x &&
		bmax.y > box.bmin.y &&
		bmax.z > box.bmin.z;
}

RTree::Entry::Entry()
{
	//FIXME: don't actually need these things
	node = NULL;
	id = -1;
	box = Box(vec3f(0.0f), vec3f(0.0f));
}

RTree::Entry::Entry(const Box& nbox, int nid) : box(nbox), id(nid)
{
}

RTree::Entry::Entry(const Box& nbox, Node* nnode) : box(nbox), node(nnode)
{
}

RTree::ToInsert::ToInsert(const Entry& e, int l) : entry(e), level(l)
{
}

void RTree::Node::debugDraw()
{
	if (level > 1)
		glColor3f(0,0,1);
	else if (level > 0)
		glColor3f(1,0,1);
	else
		glColor3f(1,1,1);
	
	for (int i = 0; i < (int)entries.size(); ++i)
		entries[i].box.draw();
			
	if (level > 0)
	{
		for (int i = 0; i < (int)entries.size(); ++i)
			entries[i].node->debugDraw();
	}
}	

RTree::EntryItem::EntryItem(Entry* e, float v) : entry(e), value(v)
{
}
		
bool RTree::EntryItem::operator<(const EntryItem& e) const
{
	return value < e.value;
}

RTree::Node::Node()
{
	level = 0; //default to leaf
	parent = NULL;
}

RTree::Node::~Node()
{
	if (level > 0)
	{
		for (int i= 0; i < (int)entries.size(); ++i)
			delete entries[i].node;
	}
}

void RTree::Node::add(const Entry& e)
{
	entries.push_back(e);
	if (level > 0)
		e.node->parent = this;
}

RTree::Box RTree::Node::calcBounds()
{
	Box b = entries[0].box;
	for (int i = 1; i < (int)entries.size(); ++i)
		b = b.bunion(entries[i].box);
	return b;
}

RTree::RTree()
{
	newNode = NULL;
	
	//FIXME: change these to make it faster
	maxEnt = 6;
	minEnt = mymax(2, (maxEnt * 4) / 10); //40% of max
	subtreeSearchSize = maxEnt/2;
	reinsertCount = mymax(2, maxEnt / 4);
	
	assert(minEnt <= maxEnt / 2);
	assert(subtreeSearchSize <= maxEnt);
	assert(reinsertCount < maxEnt);
	
	init();
}

RTree::~RTree()
{
	release();
}

void RTree::init()
{
	nextID = 0;

	root.node = new Node();
	root.node->level = 0;
	root.node->parent = NULL;
}

void RTree::release()
{
	delete root.node;
	root.node = NULL;
	assert(newNode == NULL);
	assert(toReinsert.size() == 0);
}

int RTree::chooseSubtree(Node* N, const Entry& e, int level)
{
	//assert(N->level > 0);

	//if childpointers in N point to leaves...
	if (N->entries[0].node->level == 0)
	{
		//printf("LEAF\n");
		//calculate volume enlargement for all boxes
		std::vector<EntryItem> enlarge;
		for (int i = 0; i < (int)N->entries.size(); ++i)
			enlarge.push_back(EntryItem(&N->entries[i], e.box.funion(N->entries[i].box) - N->entries[i].box.volume()));
		
		//sort by required enlarge.
		std::sort(enlarge.begin(), enlarge.end());
		
		//for the first few, find total overlap and choose the smallest.
		//TODO: implement resolve ties by area enlargement
		int smallestOverlapI = -1;
		float smallestOverlap;
		int smallestAreaI = -1;
		float smallestArea;
		for (int j = 0; j < mymin(subtreeSearchSize, (int)enlarge.size()); ++j)
		{
			//box if added to node
			Box test = e.box.bunion(enlarge[j].entry->box);
		
			//sum all overlaps with surrounding boxes in current node
			float overlap = 0.0f;
			int self;
			for (int i = 0; i < (int)N->entries.size(); ++i)
			{
				if (enlarge[j].entry != &N->entries[i])
					overlap += test.fintersect(N->entries[i].box);
				else
					self = i;
			}
			
			float area = test.volume();
					
			if (smallestOverlapI == -1 || overlap <= smallestOverlap)
			{
				smallestOverlapI = self;
				smallestOverlap = overlap;
				
				if (smallestAreaI == -1 || overlap < smallestOverlap || area < smallestArea)
				{
					smallestAreaI = self;
					smallestArea = area;
				}
			}
		}
		
		return smallestAreaI;
	}
	else
	{
		//printf("NODE\n");
		//find entry with the smallest volume enlargement to cover the new node
		int smallestEnlargeI = -1;
		float smallestEnlarge;
		for (int i = 0; i < (int)N->entries.size(); ++i)
		{
			float enlarge = e.box.funion(N->entries[i].box);
					
			if (smallestEnlargeI == -1 || enlarge < smallestEnlarge)
			{
				smallestEnlargeI = i;
				smallestEnlarge = enlarge;
			}
		}
		
		return smallestEnlargeI;
	}
}
	
int RTree::insert(const Entry& e, int level)
{
	//assert(newNode == NULL);
	//printf("Adding entry for level %i\n", level);
	//assert(level >= 0);

	//find path to best insertion
	//printf("Starting at root\n");
	std::vector<int> indexInParent;
	Node* N = root.node;
	Box* NBox = &root.box;
	while (N->level > level)
	{
		assert(N->level > 0);
		int next = chooseSubtree(N, e, level);
		//printf("Next node %i\n", next);
		indexInParent.push_back(next);
		NBox = &N->entries[next].box;
		N = N->entries[next].node;
	}
	
	//printf("Chosen level %i %s\n", level, N == root.node ? "root" : (N->level == 0 ? "leaf" : "branch"));
	
	//always just add entry
	if (N->entries.size() == 0)
	{
		//if it's first addition to the root node, simply set the bounding box
		//assert(N == root.node);
		*NBox = e.box;
	}
	else
		*NBox = NBox->bunion(e.box);
	
	if (level > 0)
	{
		//update node pointers if we're inserting a node and not data
		e.node->parent = N;
		e.node->level = level;
	}
	
	N->add(e);
	
	//go backwards through the path, updating bounds and splitting as needed
	//printf("Retracing to root\n");
	while (N)
	{
		//printf("Depth %i\n", (int)indexInParent.size());
		
		if (N->parent)
			NBox = &N->parent->entries[indexInParent.back()].box;
		else
			NBox = &root.box;
			
		if ((int)N->entries.size() >= maxEnt)
		{
			//printf("Overflow...\n");
			bool split = overflowTreatment(N, *NBox);
			if (split)
			{
				//if we're about to split the root node, a new root must be created first
				if (root.node == N)
				{
					printf("Root split!\n");
					Entry oldRoot = root;
					root.node = new Node();
					root.node->level = oldRoot.node->level + 1;
					root.node->add(oldRoot);
					oldRoot.node->parent = root.node;
					indexInParent.insert(indexInParent.begin(), 0);
				}
			
				//add the split node and calculate its bounds
				Entry newEntry;
				newEntry.node = newNode;
				newEntry.box = newNode->calcBounds();
				N->parent->add(newEntry); //NOTE: this messes with the parent's entries, which is why pointers were giving me so much grief and I switched to indexInParent
				newNode = NULL;
				
				//update pointer - it might have moved with add(newEntry)
				NBox = &N->parent->entries[indexInParent.back()].box;
			}
			//assert(newNode == NULL);
		}
		
		*NBox = N->calcBounds();
		indexInParent.pop_back();
		
		N = N->parent;
	}
	
	//reinsert any entries that were popped out (eg from overflowTreatment -> reinsert)
	if (toReinsert.size())
	{
		std::vector<ToInsert> inserting = toReinsert;
		toReinsert.clear();
		//printf("Reinserting %i\n", (int)inserting.size());
		for (int i = 0; i < (int)inserting.size(); ++i)
		{
			//printf("\tlevel %i\n", inserting[i].level);
			insert(inserting[i].entry, inserting[i].level);
		}
	}
	return 0;
}

bool RTree::overflowTreatment(Node* node, const Box& nodeBounds)
{
	if (node != root.node && firstOverflow)
	{
		//printf("Reinsert...\n");
		firstOverflow = false;
		reinsert(node, nodeBounds);
		return false;
	}
	else
	{
		//printf("Split...\n");
		split(node);
		return true;
	}
}
	
void RTree::reinsert(Node* node, const Box& nodeBounds)
{
	if ((int)node->entries.size() < minEnt + reinsertCount)
	{
		printf("Not REINSERTING\n");
		return;
	}

	//compute a list of distances between child centres bounding centre
	vec3f centre = (nodeBounds.bmin + nodeBounds.bmax) * 0.5f;
	std::vector<EntryItem> distances;
	for (int i = 0; i < (int)node->entries.size(); ++i)
	{
		float dist = ((node->entries[i].box.bmin + node->entries[i].box.bmax) * 0.5f - centre).size();
		distances.push_back(EntryItem(&node->entries[i], dist));
	}
	
	//sort by distance
	std::sort(distances.begin(), distances.end());
	
	//remove the last few
	std::set<Entry*> toRemove;
	for (int i = (int)distances.size()-reinsertCount; i < (int)distances.size(); ++i)
		toRemove.insert(distances[i].entry);
	
	for (int i = (int)node->entries.size()-1; i >= 0; --i)
	{
		if (toRemove.find(&node->entries[i]) != toRemove.end())
		{
			//printf("Adding to reinsert list\n");
			toReinsert.push_back(ToInsert(node->entries[i], node->level));
			node->entries.erase(node->entries.begin() + i);
		}
	}
}

void RTree::split(Node* node)
{
	//sort by bounds in each axis. this reminds me of SAH "events"
	std::vector<EntryItem> sortedMin[3], sortedMax[3];
	for (int i = 0; i < (int)node->entries.size(); ++i)
	{
		for (int a = 0; a < 3; ++a)
		{
			sortedMin[a].push_back(EntryItem(&node->entries[i], node->entries[i].box.bmin[a]));
			sortedMax[a].push_back(EntryItem(&node->entries[i], node->entries[i].box.bmax[a]));
		}
	}
	for (int a = 0; a < 3; ++a)
	{
		std::sort(sortedMin[a].begin(), sortedMin[a].end());
		std::sort(sortedMax[a].begin(), sortedMax[a].end());
	}
	
	//find S for each axis
	float S[3];
	float leastOverlap[3];
	int leastOverlapI[3];
	float leastArea[3];
	int leastAreaI[3];
	bool leastOverlapSort[3];
	//printf("Find Split\n");
	for (int a = 0; a < 3; ++a)
	{
		S[a] = 0.0f;
		leastOverlapI[a] = -1;
		leastAreaI[a] = -1;
		for (int k = 1; k <= (maxEnt - minEnt*2 + 2); ++k)
		{
			int d = (minEnt - 1) + k;
			//assert(d < (int)sortedMin[a].size());
			//printf("%i:%i:%i\n", 0, d, (int)sortedMin[a].size()-1);
			
			Box minFirst = sortedMin[a][0].entry->box;
			Box minSecond = sortedMin[a][d].entry->box;
			Box maxFirst = sortedMax[a][0].entry->box;
			Box maxSecond = sortedMax[a][d].entry->box;
			for (int i = 1; i < d; ++i)
			{
				minFirst = minFirst.bunion(sortedMin[a][i].entry->box);
				maxFirst = maxFirst.bunion(sortedMax[a][i].entry->box);
			}
			for (int i = d + 1; i < (int)sortedMin[a].size(); ++i)
			{
				minSecond = minSecond.bunion(sortedMin[a][i].entry->box);
				maxSecond = maxSecond.bunion(sortedMax[a][i].entry->box);
			}
			
			float minMargin = minFirst.surfaceArea() + minSecond.surfaceArea();
			float maxMargin = maxFirst.surfaceArea() + maxSecond.surfaceArea();
			S[a] += minMargin + maxMargin;
			
			float minOverlap = minFirst.fintersect(minSecond);
			float maxOverlap = maxFirst.fintersect(maxSecond);
			
			float minArea = minFirst.volume() + minSecond.volume();
			float maxArea = maxFirst.volume() + maxSecond.volume();
			
			if (leastOverlapI[a] == -1 || minOverlap <= leastOverlap[a])
			{
				if (leastAreaI[a] == -1 || minOverlap < leastOverlap[a] || minArea < leastArea[a])
				{
					leastArea[a] = minArea;
					leastAreaI[a] = d;
					leastOverlapSort[a] = false;
					//printf("%d %d %f %f\n", a, d, minOverlap, minArea);
				}
				leastOverlap[a] = minOverlap;
				leastOverlapI[a] = d;
			}
			if (maxOverlap <= leastOverlap[a])
			{
				if (leastAreaI[a] == -1 || maxOverlap < leastOverlap[a] || maxArea < leastArea[a])
				{
					leastArea[a] = maxArea;
					leastAreaI[a] = d;
					leastOverlapSort[a] = true;
					//printf("%d %d %f %f\n", a, d, maxOverlap, maxArea);
				}
				leastOverlap[a] = maxOverlap;
				leastOverlapI[a] = d;
			}
		}
	}
	
	//choose the axis with smallest S
	int axis;
	if (S[0] < S[1] && S[0] < S[2])
		axis = 0;
	else if (S[1] < S[2])
		axis = 1;
	else
		axis = 2;
	//printf("x=%f y=%f z=%f\n", S[0], S[1], S[2]);
	
	//TODO: resolve leatOverlap ties with leastArea
	int d = leastAreaI[axis];
	
	//printf("Split %i -> %i : %i\n", (int)sortedMax[axis].size(), d, (int)sortedMax[axis].size() - d);
	//printf("Axis = %i\n", axis);
	
	//make a list of things to remove, forming the second group in a new node
	std::set<Entry*> toRemove;
	if (!leastOverlapSort[axis])
	{
		for (int i = d; i < (int)sortedMin[axis].size(); ++i)
			toRemove.insert(sortedMin[axis][i].entry);
	}
	else
	{
		for (int i = d; i < (int)sortedMax[axis].size(); ++i)
			toRemove.insert(sortedMax[axis][i].entry);
	}
	
	//create the new node
	assert(newNode == NULL);
	newNode = new Node();
	newNode->level = node->level;
	
	//move the elements in the second group from the old node to the new node
	std::vector<Entry> keep;
	for (int i = (int)node->entries.size()-1; i >= 0; --i)
	{
		if (toRemove.find(&node->entries[i]) != toRemove.end())
			newNode->add(node->entries[i]);
		else
			keep.push_back(node->entries[i]);
	}
	node->entries = keep;
	
	#if 0
	printf("IN GROUP 1\n");
	for (int i = 0; i < (int)keep.size(); ++i)
	{	
		PRINTVEC3F(keep[i].box.bmin);
		PRINTVEC3F(keep[i].box.bmax);
	}
	printf("IN GROUP 2\n");
	for (int i = 0; i < (int)newNode->entries.size(); ++i)
	{	
		PRINTVEC3F(newNode->entries[i].box.bmin);
		PRINTVEC3F(newNode->entries[i].box.bmax);
	}
	#endif
}
	
void RTree::find(std::vector<int>& results, const vec3f& bmin, const vec3f& bmax)
{
	find(results, Box(bmin, bmax));
}

void RTree::find(std::vector<int>& results, const Box& box)
{
	results.clear();
	
	Box obox(vmin(box.bmin, box.bmax), vmax(box.bmin, box.bmax));
	
	if (!obox.intersects(root.box))
		return;
	
	std::vector<Node*> stack;
	stack.push_back(root.node);
	while (stack.size())
	{
		Node* n = stack.back();
		stack.pop_back();
		for (int i = 0; i < (int)n->entries.size(); ++i)
		{
			if (obox.intersects(n->entries[i].box))
			{
				if (n->level > 0)
					stack.push_back(n->entries[i].node);
				else
					results.push_back(n->entries[i].id);
			}
		}
	}
}

int RTree::insert(const vec3f& bmin, const vec3f& bmax)
{
	int id = nextID++;
	insert(Box(bmin, bmax), id);
	return id;
}

int RTree::insert(const Box& box)
{
	int id = nextID++;
	insert(box, id);
	return id;
}

void RTree::insert(const vec3f& bmin, const vec3f& bmax, int id)
{
	return insert(Box(bmin, bmax), id);
}

void RTree::insert(const Box& box, int id)
{
	firstOverflow = true;
	Box obox(vmin(box.bmin, box.bmax), vmax(box.bmin, box.bmax));
	//printf("Inserting...\n");
	//PRINTVEC3F(box.bmin);
	//PRINTVEC3F(box.bmax);
	
	/*
	std::vector<Node*> stack;
	stack.push_back(root.node);
	while (stack.size())
	{
		Node* n = stack.back();
		stack.pop_back();
		for (int i = 0; i < (int)n->entries.size(); ++i)
		{
			if (n->level > 1)
			{
				stack.push_back(n->entries[i].node);
			}
			else if (n->level == 1)
			{
				reinsert(n->entries[i].node, n->entries[i].box);
			}
		}
	}
	*/
	
	insert(Entry(obox, id), 0); //insert new data entry at leaf level
}

void RTree::clear()
{
	release();
	init();
}

void RTree::debugDraw()
{
	assert(newNode == NULL);
		
	root.node->debugDraw();
	
	glColor3f(0,1,0);
	root.box.draw();
}


