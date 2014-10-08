
#include "prec.h"

#include "vbomesh.h"
#include "util.h"
#include "trace.h"
#include "camera.h"
#include "img.h"
#include "rtree.h"
#include "imgpng.h"
#include "quaternion.h"

using namespace std;

#define GLOBAL_TRACE_DEBUG 0

bool TraceScene::SAHEvent::operator<(const SAHEvent& other) const
{
	if (pos < other.pos) return true;
	if (pos == other.pos)
	{
		if (axis < other.axis) return true;
		if (axis == other.axis && type < other.type) return true;
	}
	return false;
}

TraceScene::TraceScene()
{
	cullBackface = false;
	totalJobs = 0;
	background = vec4f(0.0f);
	defaultMaterial = new Material();
	defaultMaterial->colour = vec4f(0.6, 0.6, 0.6, 1.0f);
	materials.push_back(defaultMaterial);
	dof.samples = 1;
	gi.samples = 0;
	gi.maxDepth = 1;
	shootPhotons = false;
	photons.emit = 0;
	gloss.samples = 12;
	traversalCost = 16.0f;
	intersectCost = 1.0f;
	photonTree = NULL;
	debugMesh = NULL;
	debugMeshTrace = NULL;
	debugMeshTrace2 = NULL;
	debugMeshTrace3 = NULL;
	
	//FIXME: initialize from scene bounds?
	gi.maxDistance = 100.0f;
	photons.maxDistance = 100.0f;
	filtering.anisotropic = 16;
	
	shadowScale = 1.0f;
}
TraceScene::~TraceScene()
{
	delete defaultMaterial;
	delete photonTree;
	cancel();
}
TraceScene::Vertex TraceScene::interpolateVertex(int a, int b, int c, float s, float t)
{
	Vertex r;
	float st = 1.0 - s - t;
	r.n = vertexData[b].n * s + vertexData[c].n * t + vertexData[a].n * st;
	r.t = vertexData[b].t * s + vertexData[c].t * t + vertexData[a].t * st;
	r.ts = vertexData[b].ts * s + vertexData[c].ts * t + vertexData[a].ts * st;
	return r;
}
inline const vec2i TraceScene::textureRepeatClamp(const QI::Image* img, vec2i pos)
{
	if (img->repeat)
	{
		pos.x = ((pos.x % img->width) + img->width) % img->width;
		pos.y = ((pos.y % img->height) + img->height) % img->height;
	}
	else
	{
		pos.x = myclamp(pos.x, 0, img->width-1);
		pos.y = myclamp(pos.y, 0, img->height-1);
	}
	return pos;
}
vec4f TraceScene::texelFetch(QI::Image* img, vec2i pos)
{
	int x = 0;
	vec4f ret;
	for (; x < img->channels; ++x)
		ret[x] = img->data[(pos.y * img->width + pos.x) * img->channels + x] / 255.0f;
		
	//ret.x = pos.x / (float)img->width;
	//ret.y = pos.y / (float)img->height;
	//ret.z = 0;
	
	//handle lack of alpha
	if (x < 4)
	{
		for (; x < 3; ++x)
			ret[x] = 0.0f;
		ret.w = 1.0f;
	}
	return ret;
}
vec4f TraceScene::texture2D(MaterialTexture* texture, vec2f pos, int mipmap)
{
	if (mipmap >= (int)texture->mipmaps.size())
		mipmap = texture->mipmaps.size() - 1;
	
	assert(mipmap >= 0);
	
	QI::Image* img = texture->mipmaps[mipmap];
	
	//if (img->nearest)
	//	pos += 0.5f;
	
	pos.x *= img->width;
	pos.y *= img->height;
	vec2i ipos;
	ipos.x = (int)floor(pos.x);
	ipos.y = (int)floor(pos.y);
	pos -= ipos;
	vec2i p1 = textureRepeatClamp(img, ipos);
	
	if (img->nearest)
	{
		return texelFetch(img, vec2i(p1.x, p1.y));
	}
	else
	{
		vec2i p2 = textureRepeatClamp(img, ipos + 1);
		vec4f a = texelFetch(img, vec2i(p1.x, p1.y));
		vec4f b = texelFetch(img, vec2i(p2.x, p1.y));
		vec4f c = texelFetch(img, vec2i(p1.x, p2.y));
		vec4f d = texelFetch(img, vec2i(p2.x, p2.y));
		vec4f x1 = interpLinear(a, b, pos.x);
		vec4f x2 = interpLinear(c, d, pos.x);
		return interpLinear(x1, x2, pos.y);
	}
}
vec4f TraceScene::texture2D(MaterialTexture* texture, vec2f pos, const vec2f& dTdx, const vec2f& dTdy)
{
	vec4f col(0.0f, 0.0f, 0.0f, 0.0f);
	float mx = myabs(dTdx.x) + myabs(dTdx.y);
	float my = myabs(dTdy.x) + myabs(dTdy.y);
	
	//FIXME: this..
	if (mx != mx) {mx = 0.0f;}
	if (my != my) {my = 0.0f;}
	float m = mymin(mx, my);
	float tSize = mymax(texture->mipmaps[0]->width, texture->mipmaps[0]->height);
	float level = log2(mymax(1.0f, tSize * m / filtering.sqrtSamples));
	int ilevel = floor(level);
	if (ilevel < 0)
		printf("ERROR: ilevel %f < 0 (%s:%i)\n", level, __FILE__, __LINE__);
	level -= ilevel;
	#if 0
	printf("%i\n", level);
	col.x = level / 6.0f;
	col.w = 1.0f;
	return col;
	#endif
	for (size_t i = 0; i < filtering.samples.size(); ++i)
	{
		const vec2f& s = filtering.samples[i];
		vec4f a = texture2D(texture, pos + dTdx * s.x + dTdy * s.y, ilevel);
		vec4f b = texture2D(texture, pos + dTdx * s.x + dTdy * s.y, ilevel+1);
		col += interpLinear(a, b, level);
	}
	return col / (float)filtering.samples.size();
}

vec4f TraceScene::texture2D(MaterialTexture* texture, vec2f pos, const vec2f (&d)[4])
{
	//FIXME: should be nonzero
	int ilevel = 0;
	vec4f col(0.0f, 0.0f, 0.0f, 0.0f);
	for (size_t i = 0; i < filtering.samples.size(); ++i)
	{
		const vec2f& s = filtering.samples[i];
		vec2f x1 = interpLinear(d[0], d[3], s.x);
		vec2f x2 = interpLinear(d[1], d[2], s.x);
		vec2f p = interpLinear(x1, x2, s.y);
		col += texture2D(texture, pos + p, ilevel);
	}
	return col / (float)filtering.samples.size();
}

int TraceScene::intersectTriSquare(int axis, float pos, const vec2f& bmin, const vec2f& bmax, const vec3f& a, const vec3f& b, const vec3f& c)
{
	//WARNING: UNTESTED
	
	//0 not intersecting, 1 touching/co-planar, 2 intersecting
	bool sidea = a[axis] > pos;
	bool sideb = b[axis] > pos;
	bool sidec = c[axis] > pos;
	if (sidea == sideb && sidea == sidec)
	{
		if (a[axis] == pos && b[axis] == pos && c[axis] == pos)
			return 1; //triangle lies in the plane
		return 0; //all points of the triangle are on the same side
	}
	
	//make v1 the odd vertex, on the other side of the plane
	vec3f v0, v1, v2;
	if (sidea == sideb)
		{v0 = a; v1 = c; v2 = b;}
	else if (sidea == sidec)
		{v0 = a; v1 = b; v2 = c;}
	else //sideb == sidec
		{v0 = b; v1 = a; v2 = c;}
		
	//get edge direction vectors
	vec3f d1 = v1 - v0;
	vec3f d2 = v1 - v2;
	
	//get points of intersection
	vec3f t1 = v0 + d1 * (d1[axis] / (v0[axis] - pos));
	vec3f t2 = v2 + d2 * (d2[axis] / (v2[axis] - pos));
	
	//get 2D line equation
	vec2f start, end, dir;
	switch (axis)
	{
	case 0: start = t1.yz(); end = t2.yz(); break;
	case 1: start = t1.xz(); end = t2.xz(); break;
	case 2: start = t1.xy(); end = t2.xy(); break;
	}
	dir = end - start;
	
	//check line intersection against bounding box
	vec2f both1(bmin.x, bmax.y);
	vec2f both2(bmax.x, bmin.y);
	float tmp = end.cross(start);
	float b1 = tmp + dir.cross(bmin);
	float b2 = tmp + dir.cross(both1);
	float b3 = tmp + dir.cross(both2);
	float b4 = tmp + dir.cross(bmax);
	bool bb1 = b1 > 0.0f;
	bool bb2 = b2 > 0.0f;
	bool bb3 = b3 > 0.0f;
	bool bb4 = b4 > 0.0f;
	if (bb1 == bb2 && bb1 == bb3 && bb1 == bb4)
	{
		if (b1 == 0.0f || b2 == 0.0f || b3 == 0.0f || b4 == 0.0f)
			return 1; //line touches one or more bounding points
		return 0; //all bounding points on the same side of the line
	}
	if (start.x < bmin.x && end.x < bmin.x) return 0;
	if (start.x > bmax.x && end.x > bmax.x) return 0;
	if (start.y < bmin.y && end.y < bmin.y) return 0;
	if (start.y > bmax.y && end.y > bmax.y) return 0;
	
	//line intersects or is inside bounding box
	return 2;
}

//no idea why but this seems to be faster
inline float vecDot(const vec3f& A, const vec3f& B)
{
	return A.x * B.x +
		A.y * B.y +
		A.z * B.z;
}

bool TraceScene::intersectRayTriangle(const Ray& ray, const Triangle& triangle, HitInfo& hit)
{
	//http://softsurfer.com/Archive/algorithm_0105/algorithm_0105.htm
	const vec3f& a = triangle.a;
	const vec3f& u = triangle.u;
	const vec3f& v = triangle.v;
	const vec3f& n = triangle.n;
	
	const vec3f& start = ray.start;
	const vec3f& dir = ray.dir;

	//find normal component
	float d_ndir = vecDot(n, dir);
	
	//uncomment for single sided tests
	if (cullBackface && d_ndir >= 0.0)
		return false;

	//time of intersection
	float ri = vecDot(n, a - start) / d_ndir;
	if (ri < -0.0001 || ri > 1.0001)
		return false;

	//inside triangle?
	vec3f Pi = start + dir * ri;
	vec3f w = Pi - a;
	float d_wv = vecDot(w, v);
	float d_wu = vecDot(w, u);
	//float s = (d_uv*d_wv - d_vv*d_wu) / (d_uv*d_uv - d_uu*d_vv);
	//float t = (d_uv*d_wu - d_uu*d_wv) / (d_uv*d_uv - d_uu*d_vv);
	float s = (triangle.d_uv*d_wv - triangle.d_vv*d_wu) / triangle.uvuuvv;
	float t = (triangle.d_uv*d_wu - triangle.d_uu*d_wv) / triangle.uvuuvv;
	if (s >= 0.0f && t >= 0.0f && s + t <= 1.0)
	{
		hit.backface = (d_ndir >= 0.0);
		hit.pos = Pi;
		hit.s = s;
		hit.t = t;
		hit.time = ri;
		return true;
	}
	return false;
}

TraceScene::Bounds TraceScene::intersectVoxel(const Bounds& a, const Bounds& b)
{
	Bounds ret;
	ret.bmin = vmax(a.bmin, b.bmin);
	ret.bmax = vmin(a.bmax, b.bmax);
	return ret;
}

void TraceScene::clipTriangleBounds(SAHTriangle t, SAHSplit split, Bounds& left, Bounds& right)
{
	vec3f a = triangleData[t.triangle].a;
	vec3f b = triangleData[t.triangle].u + a;
	vec3f c = triangleData[t.triangle].v + a;
	
	bool a_side = a[split.axis] > split.pos;
	bool b_side = b[split.axis] > split.pos;
	bool c_side = c[split.axis] > split.pos;
	
	//find o = point on single side, u and v are intersecting edges
	vec3f o, u, v;
	if (b_side == c_side)
	{
		//a is on its own side. u and v are already intersecting edges
		u = triangleData[t.triangle].u;
		v = triangleData[t.triangle].v;
		o = a;
	}
	else if (a_side == c_side)
	{
		//b is on its own side
		u = -triangleData[t.triangle].u;
		v = c - b;
		o = b;
	}
	else //a_side == b_side
	{
		//c is on its own side
		v = -triangleData[t.triangle].v;
		u = b - c;
		o = c;
	}
	
	//find intersecting points
	vec3f p1 = o + u * ((split.pos - o[split.axis]) / u[split.axis]);
	vec3f p2 = o + v * ((split.pos - o[split.axis]) / v[split.axis]);
	
	//VERY IMPORTANT! floating point errors occur in the above intersection
	//this makes sure that the bounding box won't overlap the split pos
	p1[split.axis] = split.pos;
	p2[split.axis] = split.pos;
	
	//AABB of union of triangle-plane intersection points and vertices on each side of plane
	left.bmin = right.bmin = vmin(p1, p2);
	left.bmax = right.bmax = vmax(p1, p2);
	if (a_side) {right.bmin = vmin(right.bmin, a); right.bmax = vmax(right.bmax, a);}
	else        {left.bmin = vmin(left.bmin, a); left.bmax = vmax(left.bmax, a);}
	if (b_side) {right.bmin = vmin(right.bmin, b); right.bmax = vmax(right.bmax, b);}
	else        {left.bmin = vmin(left.bmin, b); left.bmax = vmax(left.bmax, b);}
	if (c_side) {right.bmin = vmin(right.bmin, c); right.bmax = vmax(right.bmax, c);}
	else        {left.bmin = vmin(left.bmin, c); left.bmax = vmax(left.bmax, c);}
	
	//intersect with bounding box (which includes previous split planes)
	left = intersectVoxel(left, t.clip);
	right = intersectVoxel(right, t.clip);
}

float TraceScene::SA(Bounds voxel)
{
	vec3f size = voxel.bmax - voxel.bmin;
	#if GLOBAL_TRACE_DEBUG
	assert(size.x >= 0.0f && size.y >= 0.0f && size.z >= 0.0f);
	#endif
	return 2.0f * (
		size.x * size.y +
		size.x * size.z +
		size.y * size.z);
}

float TraceScene::SAH(float Pl, float Pr, int Nl, int Nr)
{
	float lambda = (Nl == 0 || Nr == 0) ? 0.8f : 1.0f;
	return lambda * (traversalCost + intersectCost * (Pl * Nl + Pr * Nr));
}

void TraceScene::SAH(Bounds v, SAHSplit& split, int Nl, int Nr, int Np)
{

	Bounds left(v), right(v);
	#if GLOBAL_TRACE_DEBUG
	if (!(left.bmax[split.axis] >= split.pos))
		printf("%f >= %f\n", left.bmax[split.axis], split.pos);
	if (!(right.bmin[split.axis] <= split.pos))
		printf("%f <= %f\n", right.bmin[split.axis], split.pos);
	assert(left.bmax[split.axis] >= split.pos);
	assert(right.bmin[split.axis] <= split.pos);
	#endif
	left.bmax[split.axis] = split.pos;
	right.bmin[split.axis] = split.pos;
	
	float Pt = SA(v);
	float Pl = SA(left) / Pt;
	float Pr = SA(right) / Pt;
	
	//find the cost of putting planar triangles in both left and right voxels
	float Cp_l = SAH(Pl, Pr, Nl + Np, Nr);
	float Cp_r = SAH(Pl, Pr, Nl, Nr + Np);

	#if 0
	printf("\tPl: %.1f Pr: %.1f (Nl: %i Np: %i Nr: %i) L: %.1f, R: %.1f\n", Pl, Pr, Nl, Np, Nr, Cp_l, Cp_r);
	#endif
	
	//choose the lesser cost
	if (Cp_l < Cp_r)
	{
		split.cost = Cp_l;
		split.side = SAHSplit::LEFT;
	}
	else
	{
		split.cost = Cp_r;
		split.side = SAHSplit::RIGHT;
	}
}

void TraceScene::addEvents(std::vector<SAHEvent>& E, const SAHTriangle& t)
{
	for (int k = 0; k < 3; ++k)
	{
		if (t.clip.bmin[k] == t.clip.bmax[k])
		{
			E.push_back(SAHEvent(SAHEvent::PLANE, k, t.clip.bmin[k], t.triangle));
		}
		else
		{
			#if 0
			if (k == 0)
				printf("START: %f, END: %f\n", t.clip.bmin[k], t.clip.bmax[k]);
			#endif
			E.push_back(SAHEvent(SAHEvent::END, k, t.clip.bmax[k], t.triangle));
			E.push_back(SAHEvent(SAHEvent::START, k, t.clip.bmin[k], t.triangle));
		}
	}
}
	
void TraceScene::findSplit(Bounds voxel, std::vector<SAHEvent>& E, int totalTriangles, SAHSplit& bestSplit)
{
	#if GLOBAL_TRACE_DEBUG
	for (int i  = 0; i < (int)E.size(); ++i)
	{
		assert(E[i].pos >= voxel.bmin[E[i].axis]);
		assert(E[i].pos <= voxel.bmax[E[i].axis]);
	}
	#endif

	//most of the variable names are from:
	//http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.140.2707
	bestSplit.cost = 999999.0f;
	
	vec3i Nl(0), Nr(totalTriangles);
	int En = (int)E.size();
	int i = 0;
	while (i < En)
	{
		const SAHEvent& p = E[i];
		const int& k = p.axis;
		
		int Estart(0), Eplane(0), Eend(0);
		while (i < En && p.pos == E[i].pos && p.axis == E[i].axis && E[i].type == SAHEvent::END)
			{++Eend; ++i;}
		while (i < En && p.pos == E[i].pos && p.axis == E[i].axis && E[i].type == SAHEvent::PLANE)
			{++Eplane; ++i;}
		while (i < En && p.pos == E[i].pos && p.axis == E[i].axis && E[i].type == SAHEvent::START)
			{++Estart; ++i;}
		
		#if 0
		printf("%c Eend: %i, Eplane: %i, Estart: %i\n", 'X'+k, Eend, Eplane, Estart);
		#endif
		
		float Np = Eplane;
		Nr[k] -= Eplane + Eend;
		
		SAHSplit split;
		split.pos = p.pos;
		split.axis = k;
		SAH(voxel, split, Nl[k], Nr[k], Np);
		
		#if 0
		printf("A split: %c, %f, %s : %f\n", 'X' + split.axis, split.pos, split.side==0?"LEFT":"RIGHT", split.cost);
		#endif
	
		Nl[k] += Estart + Eplane;
		
		if (split.cost < bestSplit.cost)
			bestSplit = split;
	}
	
	#if GLOBAL_TRACE_DEBUG
	assert(bestSplit.pos >= voxel.bmin[bestSplit.axis] && bestSplit.pos <= voxel.bmax[bestSplit.axis]);
	#endif
	
	#if 0
	printf("Found best split: %c, %.2f, %s : %.2f\n", 'X' + bestSplit.axis, bestSplit.pos, bestSplit.side==0?"LEFT":"RIGHT", bestSplit.cost);
	usleep(100000);
	#endif
}

void TraceScene::doSplit(SAHSplit split, const std::vector<SAHTriangle>& T, const std::vector<SAHEvent>& E, std::vector<SAHTriangle>& Tl, std::vector<SAHEvent>& El, std::vector<SAHTriangle>& Tr, std::vector<SAHEvent>& Er)
{
	//classify triangles
	for (int i = 0; i < (int)T.size(); ++i)
		triangleData[T[i].triangle].side = SAHTriangle::BOTH;
	
	for (int i = 0; i < (int)E.size(); ++i)
	{
		const SAHEvent& e = E[i];
		if (e.axis != split.axis)
			continue;
			
		uchar& triangleSide = triangleData[e.triangle].side;
		
		if (e.type == SAHEvent::END && e.pos <= split.pos)
			triangleSide = SAHTriangle::LEFT;
		else if (e.type == SAHEvent::START && e.pos >= split.pos)
			triangleSide = SAHTriangle::RIGHT;
		else if (e.type == SAHEvent::PLANE)
		{
			if (e.pos < split.pos || (e.pos == split.pos && split.side == SAHSplit::LEFT))
				triangleSide = SAHTriangle::LEFT;
			else if (e.pos > split.pos || (e.pos == split.pos && split.side == SAHSplit::RIGHT))
				triangleSide = SAHTriangle::RIGHT;
		}
	}
	
	//split events list
	for (int i = 0; i < (int)E.size(); ++i)
	{
		uchar side = triangleData[E[i].triangle].side;
		if (side == SAHTriangle::LEFT)
			El.push_back(E[i]);
		else if (side == SAHTriangle::RIGHT)
			Er.push_back(E[i]);
		//else side == BOTH, discard event
	}
	
	vector<SAHEvent> Enl, Enr;
	
	//split triangles list
	for (int i = 0; i < (int)T.size(); ++i)
	{
		uchar side = triangleData[T[i].triangle].side;
		if (side == SAHTriangle::LEFT)
			Tl.push_back(T[i]);
		if (side == SAHTriangle::RIGHT)
			Tr.push_back(T[i]);
		if (side == SAHTriangle::BOTH)
		{
			//clip triangle to voxel and generate new events
			SAHTriangle left(T[i]);
			SAHTriangle right(T[i]);
			
			clipTriangleBounds(T[i], split, left.clip, right.clip);
			
			#if GLOBAL_TRACE_DEBUG
			//printf("l %f %f\n", left.clip.bmax[split.axis], split.pos);
			//printf("r %f %f\n", right.clip.bmin[split.axis], split.pos);
			assert(left.clip.bmax[split.axis] == split.pos);
			assert(right.clip.bmin[split.axis] == split.pos);
			#endif
			
			Tl.push_back(left);
			Tr.push_back(right);
			addEvents(Enl, left);
			addEvents(Enr, right);
		}
	}

	std::sort(Enl.begin(), Enl.end());
	std::sort(Enr.begin(), Enr.end());
	
	size_t El_mid = El.size();
	size_t Er_mid = Er.size();
	
	El.insert(El.end(), Enl.begin(), Enl.end());
	Er.insert(Er.end(), Enr.begin(), Enr.end());
	
	std::inplace_merge(El.begin(), El.begin() + El_mid, El.end());
	std::inplace_merge(Er.begin(), Er.begin() + Er_mid, Er.end());
	
	#if GLOBAL_TRACE_DEBUG
	for (int i  = 0; i < (int)El.size(); ++i)
		assert(El[i].axis != split.axis || El[i].pos <= split.pos);
	for (int i  = 0; i < (int)Er.size(); ++i)
		assert(Er[i].axis != split.axis || Er[i].pos >= split.pos);
	for (int i  = 0; i < (int)Enl.size(); ++i)
		assert(Enl[i].axis != split.axis || Enl[i].pos <= split.pos);
	for (int i  = 0; i < (int)Enr.size(); ++i)
		assert(Enr[i].axis != split.axis || Enr[i].pos >= split.pos);
	#endif
}
int TraceScene::rbuild(int depth, std::vector<SAHTriangle>& T, std::vector<SAHEvent>& E, Bounds voxel)
{
	if (treeDepth < depth)
		treeDepth = depth;

	int nodeindex = tree.size();
	tree.push_back(Node());
	//tree[nodeindex].debug = voxel;
	
	//termination condition
	bool leaf = (depth > 20) || (T.size() < 1);
	
	//find best split
	SAHSplit split;
	if (!leaf)
	{
		findSplit(voxel, E, T.size(), split);
		if (split.cost >= intersectCost * T.size())
			leaf = true;
	}
	
	//if (debug)
	//	printf("%s d%i n%i tri=%i\n", leaf?"leaf":"node", depth, nodeindex, inTriangles.size());
	
	if (leaf)
	{
		tree[nodeindex].type = 3; /* leaf */
		tree[nodeindex].a = triangles.size();
		for (int i = 0; i < (int)T.size(); ++i)
			triangles.push_back(T[i].triangle);
		tree[nodeindex].b = triangles.size();
	}
	else
	{
		//split triangle and event list at chosen split plane
		vector<SAHTriangle> Tl, Tr;
		vector<SAHEvent> El, Er;
		doSplit(split, T, E, Tl, El, Tr, Er);

		//triangles and events have been split. release parent memory		
		mystdclear(E);
		mystdclear(T);

		tree[nodeindex].type = split.axis;
		tree[nodeindex].split = split.pos;
		Bounds voxelLess = voxel;
		Bounds voxelGreater = voxel;
		voxelLess.bmax[split.axis] = split.pos;
		voxelGreater.bmin[split.axis] = split.pos;
		
		#if 0
		float Lvol = (voxelLess.bmax.x - voxelLess.bmin.x) * (voxelLess.bmax.y - voxelLess.bmin.y) * (voxelLess.bmax.z - voxelLess.bmin.z);
		float Rvol = (voxelGreater.bmax.x - voxelGreater.bmin.x) * (voxelGreater.bmax.y - voxelGreater.bmin.y) * (voxelGreater.bmax.z - voxelGreater.bmin.z);
		assert(!(Lvol == 0.0f && Tl.size() == 0));
		assert(!(Rvol == 0.0f && Tr.size() == 0));
		printf("Split gives %.2fx%i + %.2fx%i\n", Lvol, Tl.size(), Rvol, Tr.size());
		#endif
		
		int left = rbuild(depth + 1, Tl, El, voxelLess);
		int right = rbuild(depth + 1, Tr, Er, voxelGreater);
		tree[nodeindex].a = left;
		tree[nodeindex].b = right;
		
		if (debug)
		{
			switch (split.axis)
			{
			case 0:
				debugTriangles.push_back(vec3f(split.pos, voxel.bmin.y, voxel.bmin.z));
				debugTriangles.push_back(vec3f(1.0f, 0.0f, 0.0f));
				debugTriangles.push_back(vec3f(split.pos, voxel.bmax.y, voxel.bmin.z));
				debugTriangles.push_back(vec3f(1.0f, 0.0f, 0.0f));
				debugTriangles.push_back(vec3f(split.pos, voxel.bmax.y, voxel.bmax.z));
				debugTriangles.push_back(vec3f(1.0f, 0.0f, 0.0f));
				debugTriangles.push_back(vec3f(split.pos, voxel.bmin.y, voxel.bmax.z));
				debugTriangles.push_back(vec3f(1.0f, 0.0f, 0.0f));
				break;
			case 1:
				debugTriangles.push_back(vec3f(voxel.bmin.x, split.pos, voxel.bmin.z));
				debugTriangles.push_back(vec3f(0.0f, 1.0f, 0.0f));
				debugTriangles.push_back(vec3f(voxel.bmax.x, split.pos, voxel.bmin.z));
				debugTriangles.push_back(vec3f(0.0f, 1.0f, 0.0f));
				debugTriangles.push_back(vec3f(voxel.bmax.x, split.pos, voxel.bmax.z));
				debugTriangles.push_back(vec3f(0.0f, 1.0f, 0.0f));
				debugTriangles.push_back(vec3f(voxel.bmin.x, split.pos, voxel.bmax.z));
				debugTriangles.push_back(vec3f(0.0f, 1.0f, 0.0f));
				break;
			case 2:
				debugTriangles.push_back(vec3f(voxel.bmin.x, voxel.bmin.y, split.pos));
				debugTriangles.push_back(vec3f(0.0f, 0.0f, 1.0f));
				debugTriangles.push_back(vec3f(voxel.bmax.x, voxel.bmin.y, split.pos));
				debugTriangles.push_back(vec3f(0.0f, 0.0f, 1.0f));
				debugTriangles.push_back(vec3f(voxel.bmax.x, voxel.bmax.y, split.pos));
				debugTriangles.push_back(vec3f(0.0f, 0.0f, 1.0f));
				debugTriangles.push_back(vec3f(voxel.bmin.x, voxel.bmax.y, split.pos));
				debugTriangles.push_back(vec3f(0.0f, 0.0f, 1.0f));
				break;
			}
		}
	}
	return nodeindex;
}
void TraceScene::build()
{
	MyTimer timer;
	timer.time();
	
	printf("Building KD Tree\n");
	
	int totalTriangles = triangleData.size();
	
	vector<SAHTriangle> T(totalTriangles);
	vector<SAHEvent> E;
	for (int i = 0; i < totalTriangles; ++i)
	{
		T[i].clip.bmin = vmin(vmin(triangleData[i].a, triangleData[i].a + triangleData[i].u), triangleData[i].a + triangleData[i].v);
		T[i].clip.bmax = vmax(vmax(triangleData[i].a, triangleData[i].a + triangleData[i].u), triangleData[i].a + triangleData[i].v);
		T[i].triangle = i;
		
		addEvents(E, T[i]);
	}
	
	#if GLOBAL_TRACE_DEBUG
	vec3i Ts(0), Tp(0), Te(0);
	for (int i = 0; i < (int)E.size(); ++i)
	{
		if (E[i].type == SAHEvent::START) ++Ts[E[i].axis];
		if (E[i].type == SAHEvent::END) ++Te[E[i].axis];
		if (E[i].type == SAHEvent::PLANE) ++Tp[E[i].axis];
	}
	assert(Ts == Te);
	assert(Ts.x + Tp.x == (int)T.size());
	assert(Ts.y + Tp.y == (int)T.size());
	assert(Ts.z + Tp.z == (int)T.size());
	#endif
	
	std::sort(E.begin(), E.end());
	
	#if 0
	for (int i = 0; i < (int)E.size(); ++i)
		//if (E[i].axis == 2)
			printf("%.2f %c %c %i\n", E[i].pos, 'X' + E[i].axis, "-|+"[E[i].type], E[i].triangle);
	#endif
	
	if (debug)
		debugTriangles.clear();
	
	treeDepth = 0;
	rbuild(0, T, E, sceneBounds);
	
	if (debug)
	{
		debugMesh = new VBOMesh();
		debugMesh->data = (float*)&debugTriangles[0];
		debugMesh->interleaved = true;
		debugMesh->indexed = false;
		debugMesh->primitives = GL_QUADS;
		debugMesh->has[VBOMesh::VERTICES] = true;
		debugMesh->has[VBOMesh::NORMALS] = true;
		debugMesh->calcInternal();
		debugMesh->numVertices = debugTriangles.size() / 2;
		debugMesh->uninterleave(false);
		debugMesh->data = NULL;
		debugMesh->interleave();
		debugMesh->upload(false);
	}
	
	#if 1
	int numLeaves = 0;
	int numTriangles = 0;
	uint maxLeaf = 0;
	for (int i = 0; i < (int)tree.size(); ++i)
	{
		if (tree[i].type == 3)
		{
			++numLeaves;
			numTriangles += tree[i].b - tree[i].a;
			maxLeaf = mymax(tree[i].b - tree[i].a, maxLeaf);
		}
	}
	
	printf("Created KD Tree\n");
	printf("\t%i branch capacity\n", (int)tree.size());
	printf("\t%i total leaves\n", numLeaves);
	printf("\t%i total prims\n", totalTriangles);
	printf("\t%i prims in leaves\n", (int)triangles.size());
	printf("\t%i prims in leaves check\n", numTriangles);
	printf("\t%i max leaf size\n", maxLeaf);
	printf("\t%i max depth\n", treeDepth);
	printf("\tTime: %f\n", timer.time());
	#endif
}

float TraceScene::Ray::transfer(const TraceScene::HitInfo& hitInfo, const vec3f& incidence)
{
	float t = (hitInfo.pos - start).size();
	const vec3f& N = hitInfo.interp.n;
	
	for (int i = 0; i < 4; ++i)
	{
		vec3f newP = d[i].P + d[i].D * t;
		vec3f DD = incidence + d[i].D;
		DD.normalize();
		float dtdd = -N.dot(newP) / N.dot(DD);
		d[i].P += d[i].D * t + DD * dtdd;
	}
	return t;
}

void TraceScene::Ray::reflect(const TraceScene::HitInfo& surface, const vec3f& incidence, const vec3f (&curve)[4])
{
	const vec3f& N = surface.backface?-surface.interp.n:surface.interp.n;
	
	float dn = incidence.dot(N);
	for (int i = 0; i < 4; ++i)
	{
		float dndx = d[i].D.dot(N) + incidence.dot(curve[i]);
		d[i].D = d[i].D - (curve[i] * dn + N * dndx) * 2.0f;
	}
}

void TraceScene::Ray::refract(const TraceScene::HitInfo& surface, const vec3f& incidence, const vec3f (&curve)[4], float eta)
{
	const vec3f& N = surface.backface?-surface.interp.n:surface.interp.n;
	
	float dn = incidence.dot(N);
	float dis = 1.0f - eta*eta * (1.0 - dn * dn);
	float ddn = -sqrt(dis);
	float mu = eta*dn - ddn;
	
	for (int i = 0; i < 4; ++i)
	{
		float dndx = d[i].D.dot(N) + incidence.dot(curve[i]);
		float mudx = (eta - eta*eta*dn/ddn) * dndx;
		d[i].D = d[i].D * eta - (curve[i] * mu + N * mudx);
	}
}

void TraceScene::Ray::tangentDiff(const TraceScene::HitInfo& surface, const Vertex& a, const Vertex& b, const Vertex& c, vec3f (&curve)[4], vec2f (&area)[4])
{
	const vec3f& N = surface.interp.n;
	Triangle& triangle = *surface.triangle;
	for (int i = 0; i < 4; ++i)
	{
		vec3f wd = surface.pos + d[i].P - triangle.a;
		float d_wvdx = vecDot(wd, triangle.v);
		float d_wudx = vecDot(wd, triangle.u);
		vec3f bd;
		bd.x = (triangle.d_uv*d_wvdx - triangle.d_vv*d_wudx) / triangle.uvuuvv;
		bd.y = (triangle.d_uv*d_wudx - triangle.d_uu*d_wvdx) / triangle.uvuuvv;
		bd.z = surface.s + surface.t - bd.x - bd.y;
		bd.x -= surface.s;
		bd.y -= surface.t;
		curve[i] = (a.n * bd.x + b.n * bd.y + c.n * bd.z);
		curve[i] -= N * N.dot(curve[i]);
		if (surface.backface) //FIXME: this seems a little unintuitive - normal derivs are negated for backfaces for refraction and reflection
			curve[i] = -curve[i];
		area[i] = a.t * bd.x + b.t * bd.y + c.t * bd.z;
	}
}

bool TraceScene::hitSurface(vec4f& colour, Ray& ray, HitInfo& hitInfo, TraceStack& rays, int sampleOffset, int traceFlags)
{
	bool isPhoton = ((traceFlags & TRACE_PHOTON) > 0);
	bool hasBounced = (ray.mask & (Ray::REFLECT | Ray::REFRACT)) != 0;
	bool debugTrace = ((traceFlags & TRACE_DEBUG) > 0);
	bool insertPhoton = isPhoton && hasBounced;
	bool lowIntensity = ray.intensity.x + ray.intensity.y + ray.intensity.z + ray.intensity.w < 0.1f;
	bool nonPrimary = ((ray.mask & Ray::GLOBAL) > 0) || lowIntensity;
	
	//if (lowIntensity)
	//	return true;
	
	//don't use multiple texture samples for GI rays - too expensive
	bool singleSample = nonPrimary;

	//calculate normalized incidense vector
	vec3f incidence = ray.dir;
	incidence.normalize();

	//copy ray and compute differentials
	//FIXME: how the hell do I incorporate normal maps? since they may be sampled differently due to the differentials
	Ray newRay = ray;
	float distance = newRay.transfer(hitInfo, incidence);
	
	/*
	vec3f bdx, bdy; //barycentric derivatives - xyz = s, t, 1-s-t
	if (true)
	{
		Triangle& triangle = *hitInfo.triangle;
		vec3f wdx = hitInfo.pos + newRay.dx.P - triangle.a;
		vec3f wdy = hitInfo.pos + newRay.dy.P - triangle.a;
		float d_wvdx = vecDot(wdx, triangle.v);
		float d_wudx = vecDot(wdx, triangle.u);
		float d_wvdy = vecDot(wdy, triangle.v);
		float d_wudy = vecDot(wdy, triangle.u);
		bdx.x = (triangle.d_uv*d_wvdx - triangle.d_vv*d_wudx) / triangle.uvuuvv;
		bdx.y = (triangle.d_uv*d_wudx - triangle.d_uu*d_wvdx) / triangle.uvuuvv;
		bdy.x = (triangle.d_uv*d_wvdy - triangle.d_vv*d_wudy) / triangle.uvuuvv;
		bdy.y = (triangle.d_uv*d_wudy - triangle.d_uu*d_wvdy) / triangle.uvuuvv;
		bdx.z = hitInfo.s + hitInfo.t - bdx.x - bdx.y;
		bdy.z = hitInfo.s + hitInfo.t - bdy.x - bdy.y;
		bdx.x -= hitInfo.s;
		bdx.y -= hitInfo.t;
		bdy.x -= hitInfo.s;
		bdy.y -= hitInfo.t;
	}
	
	vec2f dTdx = vertexData[hitInfo.triangle->verts[1]].t * bdx.x + vertexData[hitInfo.triangle->verts[2]].t * bdx.y + vertexData[hitInfo.triangle->verts[0]].t * bdx.z;
	vec2f dTdy = vertexData[hitInfo.triangle->verts[1]].t * bdy.x + vertexData[hitInfo.triangle->verts[2]].t * bdy.y + vertexData[hitInfo.triangle->verts[0]].t * bdy.z;
	*/
	
	vec3f normalDerivs[4];
	vec2f sampleArea[4];
	newRay.tangentDiff(hitInfo, vertexData[hitInfo.triangle->verts[1]], vertexData[hitInfo.triangle->verts[2]], vertexData[hitInfo.triangle->verts[0]], normalDerivs, sampleArea);

	vec3f normal;
	
	if (debugTrace && (!isPhoton || insertPhoton))
	{
		debugMutex.lock();
		for (float x = 0; x <= 1; x += 0.1)
		{
			vec3f x1 = interpLinear(newRay.d[0].P, newRay.d[3].P, x);
			vec3f x2 = interpLinear(newRay.d[1].P, newRay.d[2].P, x);
			for (float y = 0; y <= 1; y += 0.1)
			{
				debugPoints.push_back(hitInfo.pos + interpLinear(x1, x2, y));
			}
		}
		
		for (int i = 0; i < 4; ++i)
		{
			Ray tmp = newRay;
			tmp.mask |= Ray::DEBUG1;
			tmp.d[0] = Ray::Diff(vec3f(0.0f), vec3f(0.0f));
			tmp.d[1] = Ray::Diff(vec3f(0.0f), vec3f(0.0f));
			tmp.d[2] = Ray::Diff(vec3f(0.0f), vec3f(0.0f));
			tmp.d[3] = Ray::Diff(vec3f(0.0f), vec3f(0.0f));
			tmp.start = hitInfo.pos + newRay.d[i].P + hitInfo.interp.n * 0.1;
			tmp.end = hitInfo.pos + newRay.d[i].P + (hitInfo.interp.n + normalDerivs[i] * 10.0) * 0.1;
			debugRays.push_back(tmp);
		}
		
		debugRays.push_back(newRay);
		debugRays.back().end = hitInfo.pos;
		debugMutex.unlock();
	}

	Material* material = materials[hitInfo.triangle->material];
	vec3f specularColour = material->specular;
	vec4f diffuseColour = material->colour;
	vec3f ambientColour = material->ambient;
	if (material->imgColour.mipmaps.size())
	{
		vec4f textureColour;
		if (singleSample)
			textureColour = texture2D(&material->imgColour, hitInfo.interp.t, 0);
		else
			textureColour = texture2D(&material->imgColour, hitInfo.interp.t, sampleArea);
		diffuseColour *= textureColour;
		ambientColour *= textureColour.xyz();
	}
	
	//hold up. if this was a shadow ray we just care about transparency, nothing else
	if (traceFlags & TRACE_SHADOW)
	{
		float t = 1.0f - diffuseColour.w;
		colour *= t;
		if (t < 1.0f/255.0f)
			return true;
		return false;
	}
	
	if (material->imgNormal.mipmaps.size())
	{
		vec3f biNormal = hitInfo.interp.ts.cross(hitInfo.interp.n).unit();
		vec3f tangent = hitInfo.interp.n.cross(biNormal).unit();
		if (singleSample)
			normal = texture2D(&material->imgNormal, hitInfo.interp.t, 0).xyz() * 2.0f - 1.0f;
		else
			normal = texture2D(&material->imgNormal, hitInfo.interp.t, sampleArea).xyz() * 2.0f - 1.0f;
		normal.normalize();
		normal = tangent * normal.x + hitInfo.interp.n * normal.z + biNormal * normal.y;
	}
	else
		normal = hitInfo.interp.n;
	
	//normal = hitInfo.triangle->n;

	//ratio of refractive indices		
	float eta = hitInfo.backface ? material->index : 1.0f/material->index;
	
	//a normal always opposite the direction of the ray
	//const vec3f& facingNormal = hitInfo.backface?-normal:normal;

	//child rays will copy the current ray. initialize some common attibs
	if (hitInfo.time > 0.0000001f && ray.start != hitInfo.pos)
		newRay.lastHit.clear(); //clear lastHit if ray moved
	newRay.lastHit.insert(hitInfo.triangle);
	newRay.start = hitInfo.pos;
	newRay.canary += 1;

	//shadow rays. calculate light intensity for this hit point
	vec3f lightIntensity(0.0f);
	vec3f specularIntensity(0.0f);
	vec3f specularReflectionDir;
	reflect(specularReflectionDir, incidence, normal);
	
	bool traceShadows = traceFlags & TRACE_CAMERA;
	if (material->unlit)
	{
		traceShadows = false;
		lightIntensity = vec3f(1.0f);
	}
	
	if (traceShadows)
	{
		Ray shadowRay = newRay;
		//FIXME: neet to turn off differential computation
		memset(shadowRay.d, 0, sizeof(newRay.d));
		shadowRay.mask |= Ray::SHADOW;
		HitInfo lightHit;
		for (int l = 0; l < (int)lights.size(); ++l)
		{
			float randomAngle = UNIT_RAND * 2.0f * pi;
			vec2f randomRotate;
			vec2f randomOffset;
			if (lights[l].square)
				randomOffset = vec2f(UNIT_RAND * 2.0f, UNIT_RAND * 2.0f);
			else
				randomRotate = vec2f(cos(randomAngle), sin(randomAngle));
	
			int startSample = 0;
			int endSample = ceil((int)lights[l].samples.size(), dof.samples);
	
			//use only one random sample for each light if this is GI contribution
			if (nonPrimary)
			{
				startSample = rand() % lights[l].samples.size();
				endSample = startSample + 1;
			}
	
			//shoot shadow rays towards light
			for (int sampleIndex = startSample; sampleIndex < endSample; ++sampleIndex)
			{
				int s = (sampleOffset + sampleIndex) % lights[l].samples.size();
	
				//randomize samples
				vec2f offset = lights[l].samples[s];
				if (lights[l].square)
				{
					offset += randomOffset;
					if (offset.x > 1.0f) offset.x -= 2.0f;
					if (offset.y > 1.0f) offset.y -= 2.0f;
				}
				else
					offset = vec2f(offset.x * randomRotate.x + offset.y * randomRotate.y, -offset.x * randomRotate.y + offset.y * randomRotate.x);

				//initialize shadow ray
				shadowRay.start = newRay.start;
				shadowRay.lastHit = newRay.lastHit;
				shadowRay.end = lights[l].transform * vec4f(offset, 0.0f, 1.0f);
		
				//bring back the light sample a little just in case it's in the plane of a triangle
				shadowRay.dir = shadowRay.end - shadowRay.start;
				shadowRay.dir *= 0.9999;
				shadowRay.end = shadowRay.start + shadowRay.dir;
		
				//diffuse component for each sample is standard N dot L
				vec3f lightDir = shadowRay.dir;
				lightDir.normalize();
				float diffuseScalar = mymax(0.0f, normal.dot(lightDir));
		
				//don't bother for back facing light contribution
				if (diffuseScalar == 0.0f)
					continue;
		
				//keep intensity value when traversing light occluders - transparency contributes colour
				//note: intensity.w represents total visibility, used to mix occluder colour
				vec4f intensity = vec4f(lights[l].intensity, 1.0f);
			
				bool found = false;
				if (shadowRay.dir.sizesq() > 0.001)
					found = trace(intensity, shadowRay, lightHit, rays, sampleOffset, TRACE_SHADOW | (debugTrace?TRACE_DEBUG:0));
				
				/*
				if ((shadowRay.end - lightHit.pos).sizesq() < 0.001)
					break;
		
				Material* occluderMat = materials[lightHit.triangle->material];
				vec4f occluderCol = material->colour;
				if (occluderMat->imgColour)
					occluderCol *= texture2D(occluderMat->imgColour, lightHit.interp.t);
				float transmit = 1.0f - occluderCol.w;
		
				//intensity = vec4f((intensity * transmit + occluderCol.xyz() * occluderCol.w * intensity.w) * transmit, intensity.w * transmit);
				intensity *= transmit;
		
				if (intensity.w < 1.0f/256.0f)
				{
					foundOpaque = true;
					break;
				}
				if (lightHit.time > 0.0000001f && shadowRay.start != lightHit.pos)
					shadowRay.lastHit.clear();
				shadowRay.lastHit.insert(lightHit.triangle);
				shadowRay.start = lightHit.pos;
				*/
				
				//FIXME: rem when delete shadowScale
				intensity = intensity * shadowScale + vec4f(lights[l].intensity, 1.0f) * (1.0f - shadowScale);
				
				//add specular intensity for the hit
				float specularScalar = pow(mymax(0.0f, specularReflectionDir.dot(lightDir)), material->shininess);
				specularIntensity += intensity.xyz() * specularScalar;
		
				//add diffuse intensity
				lightIntensity += intensity.xyz() * diffuseScalar;
			}
	
			//normalize number of samples
			float intensityScalar = 1.0f / (endSample - startSample);
			lightIntensity *= intensityScalar;
			specularIntensity *= intensityScalar;
		}
	}

	//apply absorbtion
	if (hitInfo.backface && material->transmits)
	{
		newRay.intensity.x *= exp(-(1.0f - material->transmit.x) * distance * material->density);
		newRay.intensity.y *= exp(-(1.0f - material->transmit.y) * distance * material->density);
		newRay.intensity.z *= exp(-(1.0f - material->transmit.z) * distance * material->density);
	}
	
	
	
	//find tangents to the reflect direction
	vec3f u, v;
	createTangents(normal, u, v);
		
	//random rotation so gloss rays don't align
	float a = UNIT_RAND*2.0f*pi;
	float ca = cos(a);
	float sa = sin(a);
	
	int glossRays = material->gloss < 1.0f ? gloss.samples : 1;
	vec4f glossIntensity = newRay.intensity * vec4f(vec3f(1.0f / glossRays), 1.0f);
	for (int i = 0; i < glossRays; ++i)
	{
		vec3f glossyNormal;			
		if (glossRays > 1)
		{
			vec3f sampleDir = gloss.normalOffsets[i];
			sampleDir.x *= 1.0f - material->gloss;
			sampleDir.y *= 1.0f - material->gloss;
			sampleDir.normalize();
			glossyNormal = vec3f(sampleDir.x * ca + sampleDir.y * sa, -sampleDir.x * sa + sampleDir.y * ca, sampleDir.z);
			glossyNormal = u * glossyNormal.x + v * glossyNormal.y + normal * glossyNormal.z;
			glossyNormal.normalize();
			newRay.intensity = glossIntensity;
		}
		else
			glossyNormal = normal;

		//FIXME: something's wrong here when there's no material->transmits
		//calculate fresnel ratio
		float reflectance;
		if (material->transmits)// && material->reflects)
		{
			if (hitInfo.backface)
				fresnel(reflectance, incidence, -hitInfo.interp.n, material->index, 1.0f);
			else
				fresnel(reflectance, incidence, hitInfo.interp.n, 1.0f, material->index);
			//reflectance = pow(reflectance, 1.0f/1.2f);
		}
		else
			reflectance = material->reflects ? 1.0f : 0.0f;

		//create reflect ray
		if ((material->reflects || (material->transmits && ((ray.mask & Ray::REFRACT) > 0))) && reflectance > 0.0f)
		{
			vec3f reflectDir;
			reflect(reflectDir, incidence, glossyNormal);
	
			if (reflectDir.dot(hitInfo.triangle->n) < 0.0)
			{
				reflect(reflectDir, reflectDir, hitInfo.triangle->n);
				//newRay.intensity.x = 1.0f; //for debugging
			}
	
			Ray reflectRay = newRay;
			reflectRay.mask |= Ray::REFLECT;
			reflectRay.end = reflectRay.start + reflectDir * 100.0f;
			if ((ray.mask & Ray::REFRACT) > 0) //FIXME: sets reflect colour to white if already refracted for TIR. physically OK?
				reflectRay.intensity = newRay.intensity * reflectance;
			else
				reflectRay.intensity = newRay.intensity * vec4f(material->reflect, 1.0) * reflectance;
			reflectRay.depth += 1;
			reflectRay.reflect(hitInfo, incidence, normalDerivs);
			rays.push(reflectRay);
	
			newRay.intensity *= (1.0 - reflectance);
		}

		//create refract ray
		if (material->transmits && reflectance < 1.0f)
		{
			vec3f refractDir;
			refract(refractDir, incidence, hitInfo.backface?-glossyNormal:glossyNormal, eta);
	
			if (refractDir.dot(hitInfo.backface?-hitInfo.triangle->n:hitInfo.triangle->n) > 0.0)
			{
				reflect(refractDir, refractDir, hitInfo.triangle->n);
				newRay.intensity.x = 1.0f; //for debugging
			}
		
			Ray refractRay = newRay;
			refractRay.mask |= Ray::REFRACT;
			refractRay.end = refractRay.start + refractDir * 100.0f;
			refractRay.intensity = newRay.intensity;
			refractRay.depth += 1;
			refractRay.refract(hitInfo, incidence, normalDerivs, eta);
			/*for (int i = 0; i < 4; ++i)
			{
				if (hitInfo.backface)
					refract(refractRay.d[i].D, refractRay.d[i].D + incidence, -hitInfo.interp.n - normalDerivs[i], eta);
				else
					refract(refractRay.d[i].D, refractRay.d[i].D + incidence, hitInfo.interp.n + normalDerivs[i], eta);
				refractRay.d[i].D -= refractDir;
			}*/
			rays.push(refractRay);
	
			newRay.intensity = vec4f(0.0f);
		}
	} //end glossy rays

	if (traceFlags & TRACE_CAMERA)
	{
		//lightIntensity = vec3f(0.0f);
	
		//sample photons
		if (photonTree && photonPoints.size())
		{
			//FIXME: replace with derivative bounds
			const float rayRadius = 0.00001;//(newRay.d[0].P.size() + newRay.d[1].P.size() + newRay.d[2].P.size() + newRay.d[3].P.size()) * 0.25f;
			float falloff = 6.0;
			std::vector<int> results;
			//KDTreeIterator iter = photonTree->find((float*)&hitInfo.pos, pr);
			photonTree->find(results, hitInfo.pos - rayRadius, hitInfo.pos + rayRadius);
			
			for (int i = 0; i < (int)results.size(); ++i)
			{
				int n = results[i];
				vec3f& pos = photonPoints[n];
				Photon& info = photonInfo[n];
				//fixme: light scattering from solid object should be brighter?
				float backScale = info.hit.backface?1.0f:-1.0f;
				float diffuseScalar = mymax(0.0f, backScale * normal.dot(info.dir));
				float dist = falloff * (pos - hitInfo.pos).size() / info.radius;
				float att = mymax(0.0, 1.0 / (1.0 + dist) - 1.0 / (1.0 + falloff));
				//colour += newRay.intensity * vec4f(info.colour * diffuseScalar * att, 0.0f);// / (float)photons.emit;
				lightIntensity += vec3f(info.colour * diffuseScalar * att);
			}
		}
		
		vec4f col = newRay.intensity * vec4f(ambientColour + diffuseColour.xyz() * lightIntensity + specularColour * specularIntensity, diffuseColour.w);
		
		#if 1
		if (ray.mask & Ray::GLOBAL)
			colour += vec4f(col.xyz() * col.w, 0.0f);
		else
			colour += vec4f(col.xyz() * col.w, col.w);
		#endif
	}
	
	if (insertPhoton)
	{
		Photon p;
		//FIXME: replace with derivative bounds
		p.radius = mymax(mymax(newRay.d[0].P.size(), newRay.d[1].P.size()), mymax(newRay.d[2].P.size(), newRay.d[3].P.size()));
		p.colour = newRay.intensity * 1.0 / p.radius;
		p.dir = incidence;
		p.hit = hitInfo;
		photonPoints.push_back(hitInfo.pos);
		photonInfo.push_back(p);
	}

	//emit global illumination rays
	//if the ray intensity is low, don't bother
	//if the material reflects, the surfaceTexture or "gloss" parameter takes over. dont' trace GI
	//if the ray is already a GI ray, only emit more if below the max depth
	bool traceGI = gi.samples > 0 && gi.maxDepth > 0;
	if (traceGI && diffuseColour.w > 1.0f/255.0f && !material->reflects && (traceFlags & TRACE_CAMERA) && (!(ray.mask & Ray::GLOBAL) || ray.depth < gi.maxDepth))
	{
		vec4f giBaseIntensity = newRay.intensity * vec4f(diffuseColour.xyz() * diffuseColour.w, diffuseColour.w);
	
		Ray giRay = newRay;
		if (giRay.mask & Ray::GLOBAL)
			giRay.depth += 1; //already a GI ray, it's recursing exponentially
		else
			giRay.depth = 1; //reset depth count - it's just become a GI ray
		giRay.mask |= Ray::GLOBAL;
	
		//find tangents to the normal
		vec3f u, v;
		createTangents(normal, u, v);
	
		//random rotation so gi rays don't align
		float a = UNIT_RAND*2.0f*pi;
		float ca = cos(a);
		float sa = sin(a);
	
		int samplesEmitted = 0;
		int numSamples = ceil((int)gi.samplesHemisphere.size(), dof.samples); //can do both dof and GI at the same time
		float giScale = ((float)gi.samplesHemisphere.size() / (float)numSamples) / gi.totalDiffuse;
		for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
		{
			++samplesEmitted;
			int i = (sampleOffset + sampleIndex) % gi.samplesHemisphere.size();
			//vec2f rotoffset(offset.x * ca + offset.y * sa, -offset.x * sa + offset.y * ca);
			vec3f baseDir = gi.samplesHemisphere[i];
			vec3f dir(baseDir.x * ca + baseDir.y * sa, -baseDir.x * sa + baseDir.y * ca, baseDir.z);
			dir = u * dir.x + v * dir.y + normal * dir.z;
			dir.normalize();
			float diffuseScalar = dir.dot(normal);
			//assert(diffuseScalar >= 0.0f);
			//if (diffuseScalar < 1.0f/255.0f)
			//	continue;
			giRay.end = giRay.start + dir * gi.maxDistance;
			//giRay.intensity = vec4f(newRay.intensity.xyz() * diffuseColour.xyz() * (diffuseColour.w * diffuseScalar * giScale), newRay.intensity.w * diffuseColour.w);
			giRay.intensity = vec4f(giBaseIntensity.xyz() * (diffuseScalar * giScale), giBaseIntensity.w);
			rays.push(giRay);
			
			//printf("%i: %i %f %f %f %f\n", giRay.depth, samplesEmitted, giRay.intensity.x, giRay.intensity.y, giRay.intensity.z, giRay.intensity.w);
		}
	}
	
	//continue tracing if transparent
	if (diffuseColour.w < 1.0f)
	{
		//remove intensity from this material
		newRay.intensity *= 1.0f - diffuseColour.w;
		
		ray.mask |= Ray::TRANSPARENT;
		ray.intensity = newRay.intensity;
		
		//don't increment depth - we still want to emit GI rays
		//transparentRay.depth += 1;
		//transparentRay.intensity.x -= col.w;
		//transparentRay.intensity.y -= col.w;
		//transparentRay.intensity.z -= col.w;
		return false; //we're not done. keep going
	}
	
	//ray.intensity = vec4f(0.0f);
	return true; //we're done with this ray. stop tracing along it
}

static bool hitInfoCompare(const TraceScene::HitInfo& a, const TraceScene::HitInfo& b)
{
	return a.time < b.time;
}

bool TraceScene::trace(vec4f& colour, Ray& ray, HitInfo& hitInfo, TraceStack& rays, int sampleOffset, int traceFlags)
{
	std::stack<TraceInterval> stack;
	stack.push(TraceInterval(0, 0.0f, 1.0f));
	while (stack.size())
	{
		int node = stack.top().node;
		float start = stack.top().start;
		float end = stack.top().end;
		//int depth = stack.top().depth;
		
		#if GLOBAL_TRACE_DEBUG
		for (int i = 0; i < depth; ++i)
			printf("%c", "_+=#-|@"[i%7]);
		printf("enter %i, %f->%f (depth %i)\n", node, start, end, depth);
		#endif
		
		stack.pop();
		
		if (tree[node].type == 3)
		{
			#if 0
			Bounds& b = tree[node].debug;
			if (debug && !debugMeshTrace)
			{
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmax.z));
		
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmax.z));
		
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmax.z));
		
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmax.z));
		
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmin.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmin.z));
		
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmin.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmin.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmax.x, b.bmax.y, b.bmax.z));
				debugTriangles.push_back(vec3f(b.bmin.x, b.bmax.y, b.bmax.z));
			}
			#endif
			
			HitInfo testHit;
			vector<HitInfo> hits;
			for (uint i = tree[node].a; i < tree[node].b; ++i)
			{
				Triangle& t = triangleData[triangles[i]];
				#if 0
				if (debug && !debugMeshTrace2)
				{
					PRINTVEC3(b.bmin);
					PRINTVEC3(b.bmax);
					debugTriangles2.push_back((b.bmin + b.bmax) * 0.5f);
					debugTriangles2.push_back(t.a + (t.u + t.v) / 3.0f);
				
					debugTriangles3.push_back(t.a);
					debugTriangles3.push_back(t.n);
					debugTriangles3.push_back(t.a+t.u);
					debugTriangles3.push_back(t.n);
					debugTriangles3.push_back(t.a+t.v);
					debugTriangles3.push_back(t.n);
				}
				#endif
				if (ray.lastHit.find(&t) != ray.lastHit.end())
					continue;
				if (intersectRayTriangle(ray, t, testHit) && testHit.time > start && testHit.time <= end)
				{
					testHit.triangle = &t;
					hits.push_back(testHit);
				}
			}
			
			std::sort(hits.begin(), hits.end(), hitInfoCompare);
			
			for (size_t i = 0; i < hits.size(); ++i)
			{
				HitInfo& hit = hits[i];
				//ray.lastHit.insert(hit.triangle);
				hit.interp = interpolateVertex(hit.triangle->verts[0], hit.triangle->verts[1], hit.triangle->verts[2], hit.s, hit.t);
				hit.interp.n.normalize();
				
				#if GLOBAL_TRACE_DEBUG
				printf("FOUND\n");
				#endif
				
				if (hitSurface(colour, ray, hit, rays, sampleOffset, traceFlags))
					return true;
				//colour += 0.1f;
			}
		}
		else
		{
			const int& axis = tree[node].type;
			const float& pos = tree[node].split;
		
			const float& grad = ray.dir[axis];
			float startPos = ray.start[axis] + grad * start;
			float endPos = ray.start[axis] + grad * end;
		
			bool flip = grad < 0.0f;
			uint startBranch = flip ? tree[node].b : tree[node].a;
			uint endBranch = flip ? tree[node].a : tree[node].b;
		
			bool a_eq = startPos == pos;
			bool b_eq = endPos == pos;
			bool a = startPos > pos;
			bool b = endPos > pos;
			bool intersects = (grad != 0.0) && (a != b || a_eq || b_eq);
		
			float mid = end;
			if (intersects)
				mid = myclamp((pos - ray.start[axis]) / grad, start, end);
				
			if (flip != b || b_eq) //add end first because it's a stack
				stack.push(TraceInterval(endBranch, intersects?(mid-0.00001f):start, end));
				
			if (flip == a || a_eq) //now the near branch will be searched first
				stack.push(TraceInterval(startBranch, start, intersects?(mid+0.00001f):end));
		
			#if GLOBAL_TRACE_DEBUG
			if (intersects)
				assert(start <= mid && mid <= end);
			assert((flip == a || a_eq) || (flip != b || b_eq));
			#endif
		
			#if GLOBAL_TRACE_DEBUG
			if (flip == a || a_eq)
			{
				for (int i = 0; i < depth; ++i)
					printf("%c", "_+=#-|@"[i%7]);
				printf("near: %f->%f (%i)\n", start, intersects?mid:end, startBranch);
			}
			#endif
		
			#if 0
			if (intersects)
			{
				if (debug && !debugMeshTrace)
				{
					vec3f intPos = ray.start + ray.dir * mid;
					debugTriangles.push_back(intPos);
					debugTriangles.push_back(intPos + vec3f(0.01,0,0));
					debugTriangles.push_back(intPos + vec3f(0.01,0.01,0));
					debugTriangles.push_back(intPos);
					debugTriangles.push_back(intPos + vec3f(0.01,0,0));
					debugTriangles.push_back(intPos + vec3f(0.01,0,0.01));
				}
			}
			#endif
		
		
			#if GLOBAL_TRACE_DEBUG
			if (flip != b || b_eq)
			{
				for (int i = 0; i < depth; ++i)
					printf("%c", "_+=#-|@"[i%7]);
				printf("far: %f->%f (%i)\n", intersects?mid:start, end, endBranch);
			}
			#endif
		
		}
	}
	
	return false;
}
void TraceScene::addLight(mat44 transform, vec3f intensity, int samples, float radius, bool square)
{
	Light light;
	light.center = vec3f(transform * vec4f(0.0f, 0.0f, 0.0f, 1.0f));
	if (samples > 1)
	{
		if (square)
			poissonSquare(light.samples, samples);
		else
			poissonDisc(light.samples, samples);
		//for (int i = 0; i < (int)poisson.size(); ++i)
		//	light.samples.push_back(vec3f(transform * vec4f(poisson[i] * radius, 0.0f, 1.0f)));
	}
	else
	{
		light.samples.push_back(vec2f(0.0f));
		radius = 0.0f;
	}
	light.transform = transform * mat44::scale(radius, radius, 1.0f);
	light.intensity = intensity;// / (float)light.samples.size();
	light.square = square;
	lights.push_back(light);
}
void TraceScene::addMesh(VBOMesh* mesh, mat44 transform)
{
	if (mesh->primitives != GL_TRIANGLES)
	{
		printf("Error: Cannot raytrace non-triangle vbomesh.\n");
		return;
	}
	if (!mesh->data && !mesh->sub[VBOMesh::VERTICES])
	{
		printf("Error: TraceScene::addMesh needs local data. load() but don't upload() before TraceScene::addMesh().\n");
		return;
	}
	if (!mesh->has[VBOMesh::VERTICES] || !mesh->has[VBOMesh::NORMALS])
	{
		printf("Error: TraceScene::addMesh needs at least vertices and normals.\n");
		return;
	}

	MyTimer timer;
	timer.time();
	
	mat44 normalMatrix = transform.inverse().transpose();
	bool hasTexCoords = mesh->has[VBOMesh::TEXCOORDS];
	bool hasTangents = mesh->has[VBOMesh::TANGENTS];
	
	int offset = triangleData.size();
	int matOffset = materials.size();
	int vertOffset = vertexData.size();
	
	//make some more room for this mesh's triangles
	int numTriangles = mesh->numIndices / 3;
	int numVertices = mesh->numVertices;
	triangleBounds.resize(offset + numTriangles);
	triangleData.resize(offset + numTriangles);
	vertexData.resize(vertOffset + numVertices);
	
	//duplicate materials. it is assumed the mesh has used MaterialCache and the material pointers remain valid
	materials.resize(matOffset + mesh->materials.size());
	for (int i = 0; i < (int)mesh->materials.size(); ++i)
	{
		int matIndex = matOffset + i;
		materials[matIndex] = dynamic_cast<Material*>(mesh->materials[i]);
		if (materials[matIndex] == NULL)
			materials[matIndex] = defaultMaterial;
		else
			materials[matIndex]->keepLocal = true;
		
		materials[matIndex]->transmits = materials[matIndex]->transmit.sizesq() != 0.0f;
		materials[matIndex]->reflects = materials[matIndex]->reflect.sizesq() != 0.0f;
		shootPhotons = shootPhotons || materials[matIndex]->transmits || materials[matIndex]->reflects;
		
		if (materials[matIndex]->imgColour.mipmaps.size() == 1)
			materials[matIndex]->imgColour.generateHostMipmaps();
		
		if (cullBackface && materials[matIndex]->transmits)
			printf("Warning: Transmissive material added with backface culling on.\n");
	}
	
	//get mesh data iterators
	InterleavedEditor<vec3f> verts = mesh->getAttrib<vec3f>(VBOMesh::VERTICES);
	InterleavedEditor<vec3f> norms = mesh->getAttrib<vec3f>(VBOMesh::NORMALS);
	InterleavedEditor<vec2f> texcs;
	InterleavedEditor<vec3f> tangents;
	if (hasTexCoords)
		texcs = mesh->getAttrib<vec2f>(VBOMesh::TEXCOORDS);
	if (hasTangents)
		tangents = mesh->getAttrib<vec3f>(VBOMesh::TANGENTS);
	
	if (offset == 0)
	{
		sceneBounds.bmin = verts[0];
		sceneBounds.bmax = verts[0];
	}
	
	std::vector<VBOMeshFaceset> meshFacesets;
	for (VBOMesh::Facesets::iterator it = mesh->facesets.begin(); it != mesh->facesets.end(); ++it)
		meshFacesets.push_back(it->second);
	
	//start copying out and processing the mesh data
	int faceset = 0;
	for (int i = 0; i < numTriangles; ++i)
	{
		int mat = 0; //default material
		while (faceset < (int)meshFacesets.size() && i*3 >= meshFacesets[faceset].endIndex)
			++faceset;
		if (faceset < (int)meshFacesets.size() && i*3 >= meshFacesets[faceset].startIndex && i*3 < meshFacesets[faceset].endIndex)
			mat = matOffset + meshFacesets[faceset].material; //triangle's material
		
		vec3f a = vec3f(transform * vec4f(verts[mesh->dataIndices[i*3+0]], 1.0f));
		vec3f b = vec3f(transform * vec4f(verts[mesh->dataIndices[i*3+1]], 1.0f));
		vec3f c = vec3f(transform * vec4f(verts[mesh->dataIndices[i*3+2]], 1.0f));
		vec3f u = b - a;
		vec3f v = c - a;
		triangleData[offset + i].a = a;
		triangleData[offset + i].u = b - a;
		triangleData[offset + i].v = c - a;
		triangleData[offset + i].n = u.cross(v).unit();
		triangleData[offset + i].d_uv = u.dot(v);
		triangleData[offset + i].d_uu = u.dot(u);
		triangleData[offset + i].d_vv = v.dot(v);
		triangleData[offset + i].uvuuvv = (triangleData[offset + i].d_uv*triangleData[offset + i].d_uv - triangleData[offset + i].d_uu*triangleData[offset + i].d_vv);
		triangleData[offset + i].material = mat;
		triangleData[offset + i].verts[0] = vertOffset + mesh->dataIndices[i*3+0];
		triangleData[offset + i].verts[1] = vertOffset + mesh->dataIndices[i*3+1];
		triangleData[offset + i].verts[2] = vertOffset + mesh->dataIndices[i*3+2];
		triangleBounds[offset + i].bmin = vmin(vmin(a, b), c);
		triangleBounds[offset + i].bmax = vmax(vmax(a, b), c);
		sceneBounds.bmin = vmin(sceneBounds.bmin, triangleBounds[offset + i].bmin);
		sceneBounds.bmax = vmax(sceneBounds.bmax, triangleBounds[offset + i].bmax);
	}
	for (int i = 0; i < numVertices; ++i)
	{
		vertexData[vertOffset + i].n = normalMatrix * norms[i];
		vertexData[vertOffset + i].n.normalize();
		vertexData[vertOffset + i].t = hasTexCoords ? texcs[i] : vec2f(0.0f);
		vertexData[vertOffset + i].ts = transform * (hasTangents ? tangents[i] : vec3f(1.0f, 0.0f, 0.0f));
	}
	printf("Time to addMesh(): %f, %i polys\n", timer.time(), numTriangles);
}

bool TraceScene::trace(vec4f& colour, TraceStack& rays, int sampleOffset, int traceFlags)
{
	while (rays.size() > 0)
	{
		Ray ray = rays.top();
		ray.dir = ray.end - ray.start;
		rays.pop();
		
		//non GI rays with small light contribution are discarded
		if (!(ray.mask & Ray::GLOBAL) && ray.intensity.x < 0.2/256.0f && ray.intensity.y < 0.2/256.0f && ray.intensity.z < 0.2/256.0f)
			continue;
		
		//just in case I've messed up
		if (ray.canary > 100)
		{
			printf("Canary died (%s):\n\t%i SHADOW\n\t%i GLOBAL\n\t%i TRANSPARENT\n\t%i REFRACT\n\t%i REFLECT\n",
				(traceFlags & TRACE_PHOTON) ? "photon" : ((traceFlags & TRACE_SHADOW) ? "shadow" : "camera"),
				(int)((ray.mask & Ray::SHADOW) > 0),
				(int)((ray.mask & Ray::GLOBAL) > 0),
				(int)((ray.mask & Ray::TRANSPARENT) > 0),
				(int)((ray.mask & Ray::REFRACT) > 0),
				(int)((ray.mask & Ray::REFLECT) > 0)
				);
			continue;
		}

		HitInfo hitInfo;
		bool found = trace(colour, ray, hitInfo, rays, sampleOffset, traceFlags);
		
		//TODO: background colour
		if (!found && !(ray.mask & Ray::GLOBAL))
		{
			float ratio = mymax(ray.dir.unit().y, 0.0f);
			vec3f sky = interpLinear(vec3f(0.7, 0.8, 1.0), vec3f(0.3, 0.4, 0.6), ratio);
			//if (ratio == 0.0)
			//	sky = vec3f(0.4, 0.2, 0.0);
			colour += vec4f(sky * ray.intensity.xyz(), 1.0f) * ray.intensity.w;
		}
	}
	
	//colour += vec4f(mymax(0.0f, 1.0f - colour.w));
	//colour.w = 1.0f;
	return true;
}

void TraceScene::traceCameraRay(vec3f start, vec3f end, vec4f& colour, int sampleOffset, bool debugTrace)
{
	if (debugTrace)
	{
		debugMutex.lock();
		printf("Clearing Debug Rays\n");
		debugPoints.clear();
		debugRays.clear();
		debugMutex.unlock();
	}
	
	static Ray::Diff nodiff(vec3f(0.0f), vec3f(0.0f));
	traceCameraRay(start, end, nodiff, nodiff, colour, sampleOffset, debugTrace);
}

void TraceScene::traceCameraRay(vec3f start, vec3f end, Ray::Diff dx, Ray::Diff dy, vec4f& colour, int sampleOffset, bool debugTrace)
{
	//debugTrace = true;

	Ray startRay;
	startRay.start = start;
	startRay.end = end;
	startRay.intensity = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
	startRay.depth = 0;
	startRay.canary = 0;
	startRay.mask = 0;
	startRay.d[0] = -dx -dy;
	startRay.d[1] = -dx + dy;
	startRay.d[2] = dx + dy;
	startRay.d[3] = dx -dy;
	TraceStack rays;
	rays.push(startRay);
	
	//resolve refraction/reflections and finally diffuse hits
	trace(colour, rays, sampleOffset, TRACE_CAMERA | (debugTrace?TRACE_DEBUG:0));
}

void TraceScene::tracePhotons()
{
	printf("Photon mapping...\n");
	
	if (!photonTree)
		photonTree = new RTree();

	//create direction distribution for photons
	int emitSphereCount = mymax(1, (int)sqrt(photons.emit/(4*pi)));
	poissonSphere(photons.emitSphere, emitSphereCount);
	
	photonPoints.clear();
	photonInfo.clear();
	
	TraceStack rays;
		
	size_t totalLightSamples = 0;
	for (size_t l = 0; l < lights.size(); ++l)
		totalLightSamples += lights[l].samples.size();
		
	int photonBatchMax = 500000;
	
	int np = 0;
	while (np < photons.emit)
	{
	
		int photonBatch = mymin(photonBatchMax, photons.emit - np);
		int photonBatchTarget = np + photonBatch;
		printf("Creating %i -> %i Photons\n", np, photonBatchTarget);
		
		//shoot photons from lights
		int p = 0;
		if (lights.size() == 0)
			photons.emitSphere.clear();
		Quat randomRot = Quat::random();
		while (np < photonBatchTarget)
		{
			for (size_t l = 0; l < lights.size(); ++l)
			{
				size_t samples = lights[l].samples.size();
				float photonRatio = totalLightSamples / (float)(photons.emit * samples);
				float photonEmitArea = 4.0f * pi * photonRatio;
				//FIXME: don't use a constant here
				float deltaAngle = atan(photonEmitArea) * 200.0f;
				for (size_t s = 0; s < samples; ++s)
				{
					Ray ray;
					ray.start = lights[l].transform * vec4f(lights[l].samples[s], 0.0f, 1.0f);
					ray.dir = (randomRot * photons.emitSphere[p]);
					ray.intensity = vec4f(vec3f(0.01f), 1.0f);
					ray.depth = 0;
					ray.canary = 0;
					ray.mask = 0;
				
					vec3f perp1 = ray.dir.cross(vec3f(0,0,1));
					if (perp1.size() < 0.1f)
						perp1 = ray.dir.cross(vec3f(0,1,0));
					perp1.normalize();
					vec3f perp2 = ray.dir.cross(perp1).unit();
				
					ray.d[0].P = ray.d[0].D = Quat(deltaAngle, perp1) * ray.dir - ray.dir;
					ray.d[1].P = ray.d[1].D = Quat(deltaAngle, perp2) * ray.dir - ray.dir;
					ray.d[2].P = ray.d[2].D = Quat(-deltaAngle, perp1) * ray.dir - ray.dir;
					ray.d[3].P = ray.d[3].D = Quat(-deltaAngle, perp2) * ray.dir - ray.dir;
				
					ray.dir *= photons.maxDistance;
					ray.end = ray.start + ray.dir;
				
					rays.push(ray);
					p = (p+1)%photons.emitSphere.size();
					if (p == 0)
						randomRot = Quat::random();
					++np;
				}
			}
		}
	
		printf("Tracing Photons...\n");
	
		//resolve refraction/reflections and finally diffuse hits
		vec4f colour(0.0f); //ignored - shooting photons, not tracing camera rays
		//FIXME: do I need a sampleOffset?
		trace(colour, rays, 0, TRACE_PHOTON | TRACE_DEBUG); //
	}
	
	printf("%i Photon Hits\n", (int)photonPoints.size());
	
	//photonTree->setPoints((float*)&photonPoints[0], photonPoints.size());
	//photonTree->rebuild();
	MyTimer qwe;
	qwe.time();
	photonTree->clear();
	for (int i = 0; i < (int)photonPoints.size(); ++i)
		photonTree->insert(photonPoints[i] - photonInfo[i].radius, photonPoints[i] + photonInfo[i].radius, i);
	printf("Took %.2fms to build the tree\n", qwe.time());
	
	printf("Photon Tree Built\n");
}

vec3f TraceScene::TraceThreadJob::get(float dx, float dy, float z)
{
	vec4f point(2.0f * (x + 0.5f) / img->width - 1.0f + dx * 2.0f, 2.0f * (y + 0.5f) / img->height - 1.0f + dy * 2.0f, z * 2.0f - 1.0f, 1.0f);
	point = view * point;
	point /= point.w;
	return point.xyz();
}

void TraceScene::performJob(TraceThreadJob& job)
{
	bool debug = (myabs(job.x-job.img->width/2) < 2) && (myabs(job.y-job.img->height/2) < 2);
	
	vec3f start = job.get(0.0f, 0.0f, 0.0f);
	vec3f end = job.get(0.0f, 0.0f, 1.0f);
	Ray::Diff dx, dy;

	//trace scene
	vec4f colour(0.0f);
	if (dof.samples <= 1)
	{
		dof.samples = 1;
		
		vec3f dir = end-start;
		float p = 0.5f;
		dx.P = job.get(p / job.img->width, 0.0f, 0.0f);
		dy.P = job.get(0.0f, p / job.img->height, 0.0f);
		dx.D = job.get(p / job.img->width, 0.0f, 1.0f) - dx.P;
		dy.D = job.get(0.0f, p / job.img->height, 1.0f) - dy.P;
		dx.P -= start;
		dy.P -= start;
		dx.D.normalize();
		dy.D.normalize();
		dir.normalize();
		dx.D -= dir;
		dy.D -= dir;
		
		traceCameraRay(start, end, dx, dy, colour, 0, debug);
	}
	else
	{
		vec3f toVec = end - start;
		float length = toVec.size();
		vec3f dir = toVec.unit();
		vec3f focus = start + dir * (dof.focus - job.cam->getClipPlanes().x) / dir.dot(job.cam->toVec());
		vec3f up = job.cam->upVec();
		vec3f right = job.cam->rightVec();
		//gaussianKernal(
		assert((int)samplesDisc.size() == dof.samples);
		
		float a = UNIT_RAND*2.0f*pi;
		float ca = cos(a);
		float sa = sin(a);
		
		dx.P = right / (float)job.img->width;
		dy.P = up / (float)job.img->height;
			
		for (int i = 0; i < (int)samplesDisc.size(); ++i)
		{
			//create rays, focusing on the "focus" point
			vec2f offset = samplesDisc[i] * dof.aperture;
			vec2f rotoffset(offset.x * ca + offset.y * sa, -offset.x * sa + offset.y * ca);
			vec3f rayStart = job.cam->getZoomPos() + right * rotoffset.x + up * rotoffset.y;
			vec3f dir = (focus - rayStart).unit();
			vec3f rayEnd = start + dir * length;
			
			dx.D = focus - dx.P - rayStart;
			dy.D = focus - dy.P - rayStart;
			dx.D.normalize();
			dy.D.normalize();
			dx.D -= dir;
			dy.D -= dir;
		
			//do the trace
			vec4f tmp(0.0f);
			traceCameraRay(rayStart, rayEnd, dx, dy, tmp, i, debug && i == 0);
			//FIXME: was getting negative values at some point
			//tmp = vmin(vmax(tmp, vec4f(0.0f)), vec4f(1.0f));
			tmp = vmax(tmp, vec4f(0.0f));
			colour += tmp;
		}
		colour /= (float)samplesDisc.size();
	}
	
	//save colour
	colour *= 255.0f;
	for (int c = 0; c < job.img->channels; ++c)
		job.img->data[(job.y*job.img->width+job.x)*job.img->channels+c] = 
			(unsigned char)myclamp((int)colour[c], 0, 255);
}

void TraceScene::run()
{
	//unpack info from render() call
	QI::Image* image = renderInfo.image;
	Camera* camera = renderInfo.camera;
	int nthreads = renderInfo.nthreads;
	
	jobMutex.lock();
	renderInfo.initializingThreads = true;
	jobMutex.unlock();
	
	//make sure the render doesn't get cancelled until sub threads have been created
	printf("c init\n");
	renderInfo.threadMutex.lock();
	printf("c locked\n");
	renderInfo.initMutex.lock();
	printf("c init locked\n");
	renderInfo.initialized = true;
	printf("c init signaling\n");
	renderInfo.initBarrier.signal();
	printf("c init signaled\n");
	renderInfo.initMutex.unlock();
	printf("c init unlocked\n");
	
	printf("Clearing Debug Rays\n");
	debugPoints.clear();
	debugRays.clear();

	//create samples for texture filtering
	if (filtering.anisotropic <= 0)
	{
		filtering.anisotropic = 1;
		filtering.samples.resize(1);
		filtering.samples[0] = vec2f(0.5f);
	}
	else
	{
		poissonSquare(filtering.samples, filtering.anisotropic);
		for (int i = 0; i < (int)filtering.samples.size(); ++i)
			filtering.samples[i] = filtering.samples[i] * 0.5 + 0.5;
	}
	filtering.sqrtSamples = mymax(1.0, sqrt((double)filtering.anisotropic));

	//shoot photons for the render
	if (shootPhotons)
		tracePhotons();
	else
		printf("No photon mapping required\n");
	
	//initialize poisson disc for dof
	if (dof.samples > 1)
	{
		samplesDisc.clear();
		poissonDisc(samplesDisc, dof.samples);
	}
	
	//create random hemisphere for GI
	if (gi.samples > 1)
	{
		gi.samplesHemisphere.clear();
		poissonHemisphere(gi.samplesHemisphere, gi.samples);
		gi.totalDiffuse = 0.0f;
		for (int i = 0; i < (int)gi.samplesHemisphere.size(); ++i)
		{
			//zero-weight samples are useless
			gi.samplesHemisphere[i] += vec3f(0, 0, 0.01f);
			gi.samplesHemisphere[i].normalize();
			
			//calculate total diffuse contribution
			gi.totalDiffuse += gi.samplesHemisphere[i].z;
		}
	}
	
	gloss.samples = mymax(gloss.samples, 1);
	if (gloss.samples > 1)
	{
	
		gloss.normalOffsets.clear();
		poissonHemisphere(gloss.normalOffsets, gloss.samples);
		gi.totalDiffuse = 0.0f;
		for (int i = 0; i < (int)gloss.normalOffsets.size(); ++i)
		{
			//zero-weight samples are useless
			gloss.normalOffsets[i] += vec3f(0, 0, 0.01f);
			gloss.normalOffsets[i].normalize();
			
			//calculate total diffuse contribution
			gloss.totalDiffuse += gloss.normalOffsets[i].z;
		}
	}

	//generate jobs for each pixel
	mat44 view = (camera->getProjection() * camera->getInverse()).inverse();
	for (int y = 0; y < image->height; ++y)
	{
		for (int x = 0; x < image->width; ++x)
		{
			TraceThreadJob job;
			job.view = view;
			job.img = image;
			job.cam = camera;
			job.x = x;
			job.y = y;
			jobs.push(job);
		}
	}
	totalJobs = jobs.size();
	
	//must not have threads already running
	assert(threads.size() == 0);
	
	//create threads
	for (int i = 0; i < nthreads; ++i)
	{
		TraceThread* thread = new TraceThread(this);
		thread->id = i;
		thread->start();
		threads.push_back(thread);
	}
	
	renderInfo.threadMutex.unlock();
	jobMutex.lock();
	renderInfo.initializingThreads = false;
	jobMutex.unlock();
	printf("c unlocked\n");
}
void TraceScene::render(QI::Image* image, Camera* camera, int nthreads)
{
	if (!image || !camera || nthreads <= 0)
		return;
	
	if (triangles.size() == 0)
	{
		printf("Warning: rendering an empty scene. Did you build()?\n");
		return;
	}
	
	dof.samples = mymax(1, dof.samples); //must be at least one, where one means no depth of field

	//cleanup current/previous render
	cancel();
	mystdclear(jobs);
	
	//save info and start main render thread
	renderInfo.image = image;
	renderInfo.camera = camera;
	renderInfo.nthreads = nthreads;
	
	renderInfo.initialized = false;
	printf("t starting\n");
	start();
	printf("t started\n");
	renderInfo.initMutex.lock();
	printf("t locked\n");
	while (!renderInfo.initialized)
	{
		printf("t waiting\n");
		renderInfo.initBarrier.wait(renderInfo.initMutex);
	}
	printf("t done\n");
	renderInfo.initMutex.unlock();
	printf("t unlocked\n");
}
void TraceScene::cancel()
{
	//cannot cancel until child threads have been spawned
	renderInfo.threadMutex.lock();
	int runningThreads = 0;
	for (int i = 0; i < (int)threads.size(); ++i)
	{
		if (threads[i]->running())
		{
			runningThreads++;
			threads[i]->requestStop = true;
		}
	}
	if (runningThreads > 0)
		printf("Cancelling %i TraceScene threads\n", runningThreads);
	renderInfo.threadMutex.unlock();
	
	wait();
}
void TraceScene::wait()
{
	//cannot wait until child threads have been spawned
	renderInfo.threadMutex.lock();
	for (int i = 0; i < (int)threads.size(); ++i)
	{
		threads[i]->wait();
		delete threads[i];
	}
	threads.clear();
	renderInfo.threadMutex.unlock();
	
	//wait for the main thread
	Thread::wait();
}
float TraceScene::getProgress()
{
	float ret;
	jobMutex.lock();
	if (renderInfo.initializingThreads)
		ret = 0.0f;
	else
		ret = 1.0f - jobs.size() / (float)mymax(1, totalJobs);
	jobMutex.unlock();
	return ret;
}

#ifndef _WIN32
#include <sys/time.h>
#include <time.h>
unsigned int timeGetTime()
{
struct timeval now;
gettimeofday(&now, NULL);
return (now.tv_sec%1000)*1000000 + now.tv_usec;
//return 0;
}
#endif

void TraceScene::TraceThread::run()
{
	bool hasJobs = true;
	TraceThreadJob job;
	while (hasJobs && !requestStop)
	{
		//get job
		//printf("%u waiting\n", id);
		DEBUGSTUFF qwe1 = {"lock", timeGetTime(), id};
		//debugThreadStuff.push_back(qwe1);
		scene->jobMutex.lock();
		DEBUGSTUFF qwe2 = {"getjob", timeGetTime(), id};
		//debugThreadStuff.push_back(qwe2);
		//printf("%u getting\n", id);
		hasJobs = !scene->jobs.empty();
		if (hasJobs)
		{
			job = scene->jobs.front();
			scene->jobs.pop();
		}
		DEBUGSTUFF qwe3 = {"gotjob", timeGetTime(), id};
		//debugThreadStuff.push_back(qwe3);
		scene->jobMutex.unlock();
		DEBUGSTUFF qwe4 = {"unlock", timeGetTime(), id};
		//debugThreadStuff.push_back(qwe4);
		
		//perform job
		//printf("%u running\n", id);
		if (hasJobs)
			scene->performJob(job);
		//printf("%u done\n", id);
	}
}

bool compareQWE(TraceScene::TraceThread::DEBUGSTUFF a, TraceScene::TraceThread::DEBUGSTUFF b)
{
	return a.time < b.time;
}

void TraceScene::test()
{
	MyTimer timer;
	timer.time();
	
	debug = false;
	//cullBackface = true;

	VBOMesh cube = VBOMesh::cube();
	VBOMesh sphere = VBOMesh::grid(vec2i(32, 16) * 1, VBOMesh::paramSphere);
	VBOMesh sphere2 = VBOMesh::grid(vec2i(32, 16) * 1, VBOMesh::paramSphere);
	cube.invertNormals();
	sphere.triangulate();
	sphere2.triangulate();
	
	QI::Image checker;
	checker.resize(8, 8);
	checker.generateChecker(127);
	checker.nearest = true;
	checker.repeat = false;
	
	Material* matDefault = new Material();
	Material* matReflect = new Material();
	Material* matRefract = new Material();
	matDefault->imgColour = checker;
	matReflect->reflect = vec3f(1.0f);
	//matReflect->gloss = 0.97f;
	matRefract->reflect = vec3f(1.0f);
	matRefract->transmit = vec3f(0.2f, 0.4f, 0.9f);
	matRefract->density = 0.5f;
	matRefract->index = 1.2f;
	//matRefract->gloss = 0.97f;
	cube.setMaterial(matDefault);
	cube.transform(mat44::scale(-1));
	sphere.setMaterial(matReflect);
	sphere2.setMaterial(matRefract);
	
	Camera cam;
	cam.setPerspective(pi*0.5f);
	cam.zoomAt(vec3f(0,0,2), vec3f(0));
	cam.regen();
	
	addMesh(&cube, mat44::scale(10));
	addMesh(&sphere, mat44::translate(1,0,0) * mat44::scale(0.8));
	addMesh(&sphere2, mat44::translate(-1,0,0) * mat44::scale(0.8));
	
	addLight(mat44::translate(0,4,0), vec3f(1), 1, 0.0f, true);
	
	build();
	
	photons.emit = 100000;
	gi.samples = 0;
	gi.maxDepth = 1;
	dof.samples = 0;
	dof.focus = 1;
	dof.aperture = 0.002f;
	
	QI::ImagePNG out;
	out.resize(512, 512);
	render(&out, &cam, 4);
	printf("Rendering...\n");
	while (true)
	{
		float p = getProgress();
		printf("%.2f%%\n", p*100.0f);
		if (p == 1.0f)
			break;
		mysleep(1.0f);
	}
	//wait();
	printf("Done\n");
	out.saveImage("render.png");
	
	sphere.release();
	cube.release();
	
	printf("Total Time for Test Render: %fms\n", timer.time());
	
	std::vector<TraceThread::DEBUGSTUFF> log;
	for (size_t t = 0; t < threads.size(); ++t)
	{
		for (size_t v = 0; v < threads[t]->debugThreadStuff.size(); ++v)
			log.push_back(threads[t]->debugThreadStuff[v]);
	}
	std::sort(log.begin(), log.end(), compareQWE);
	for (std::vector<TraceThread::DEBUGSTUFF>::iterator it = log.begin(); it != log.end(); ++it)
	{
		printf("%i %i %s\n", it->time, it->id, it->doing.c_str());
	}
}

