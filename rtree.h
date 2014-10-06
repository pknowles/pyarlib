#ifndef PYARLIB_RTREE_H
#define PYARLIB_RTREE_H

#include "vec.h"

#include <vector>
#include <list>
	
class RTree
{
public:
	struct Box {
		vec3f bmin;
		vec3f bmax;
		void draw();
		Box();
		Box(const vec3f& nbmin, const vec3f& nbmax);
		float volume() const;
		float edgeLength() const;
		float surfaceArea() const;
		float funion(const Box& box) const;
		float fintersect(const Box& box) const;
		Box bunion(const Box& box) const;
		bool intersects(const Box& box) const;
	};
private:
	struct Node;
	struct Entry {
		Box box;
		union {
			int id;
			Node* node;
		};
		Entry();
		Entry(const Box& nbox, int nid);
		Entry(const Box& nbox, Node* nnode);
	};
	struct Node {
		int level;
		Node* parent;
		std::vector<Entry> entries;
		Node();
		virtual ~Node();
		void add(const Entry& e);
		Box calcBounds();
		void debugDraw();
	};
	struct EntryItem
	{
		Entry* entry;
		float value;
		EntryItem(Entry* e, float v);
		bool operator<(const EntryItem& e) const;
	};
	struct ToInsert
	{
		Entry entry;
		int level;
		ToInsert(const Entry& e, int l);
	};
	bool firstOverflow;
	int nextID;
	Entry root;
	Node* newNode;
	std::vector<ToInsert> toReinsert;
private:
	void init();
	void release();
	int chooseSubtree(Node* N, const Entry& e, int level);
	int insert(const Entry& e, int level);
	bool overflowTreatment(Node* node, const Box& nodeBounds);
	void reinsert(Node* node, const Box& nodeBounds);
	void split(Node* node);
public:

	//CHANGE THESE AT YOUR OWN PERIL
	int subtreeSearchSize;
	int reinsertCount;
	int minEnt;
	int maxEnt;
	
	RTree();
	virtual ~RTree();
	void find(std::vector<int>& results, const vec3f& bmin, const vec3f& bmax);
	void find(std::vector<int>& results, const Box& box);
	int insert(const vec3f& bmin, const vec3f& bmax);
	int insert(const Box& box);
	void insert(const vec3f& bmin, const vec3f& bmax, int id);
	void insert(const Box& box, int id);
	void clear();
	void debugDraw();
};

#endif
