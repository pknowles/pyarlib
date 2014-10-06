
#ifndef MESH_TRACER_H
#define MESH_TRACER_H

//builds a static KD tree for a triangle mesh (not realtime), and provides
//TraceScene::trace() to find triangle intersections

//http://www.flipcode.com/archives/Raytracing_Topics_Techniques-Part_7_Kd-Trees_and_More_Speed.shtml
//	^not-described-horribly

#include "vec.h"
#include "material.h"
#include "thread.h"

//TODO: stop people from using windows libraries!
#undef TRANSPARENT

#ifndef uint
typedef unsigned int uint;
#endif
#ifndef ushort
typedef unsigned short ushort;
#endif
#ifndef uchar
typedef unsigned char uchar;
#endif

struct VBOMesh;
class Camera;

namespace QI {
	struct Image;
};

class RTree;

class TraceScene : protected Thread
{
public:
	struct Bounds {
		vec3f bmin;
		vec3f bmax;
	};
	struct Node
	{
		uchar type; //x, y, z, leaf (0-3)
		uint a, b; //if leaf, triangles[a->b], else left/right pointers
		float split; //split plane along axis
		//Bounds debug;
		Node() : type(0), a(0), b(0), split(0.0f)
		{
		}
	};
	struct Triangle {
		//stores point a, edges u/v and normal n
		vec3f a, u, v, n;
		int verts[3]; //per-vertex data
		int material; //for faster lookup. don't want to search facesets.
		
		//for SAH kd tree construction
		uchar side;
		
		//cached stuff for quick barycentric calcs
		float d_uu;
		float d_vv;
		float d_uv;
		float uvuuvv;
	};
	struct Vertex {
		vec3f n;
		vec2f t;
		vec3f ts;
	};
	struct SAHTriangle {
		enum SplitSide {
			LEFT,
			RIGHT,
			BOTH
		};
		Bounds clip;
		uint triangle;
	};
	struct SAHEvent {
		enum Type {
			END, //triangle bounds end. FIRST!
			PLANE, //triangle is in plane
			START //start of triangle bounds
		};
		uint triangle;
		float pos;
		uchar type, axis;
		SAHEvent(Type t, uchar ax, float p, uint tri) : triangle(tri), pos(p), type(t), axis(ax) {}
		bool operator<(const SAHEvent& other) const;
	};
	struct SAHSplit {
		enum PlanarSide {
			LEFT,
			RIGHT
		};
		float cost;
		float pos;
		int axis;
		int side;
	};
	struct TraceInterval {
		uint node;
		float start, end;
		TraceInterval(uint n, float s, float e) : node(n), start(s), end(e) {}
	};
	struct TraceThreadJob {
		int x, y;
		mat44 view;
		QI::Image* img;
		Camera* cam;
		vec3f get(float dx, float dy, float z);
	};
	struct TraceThread : Thread {
		int id;
		TraceScene* scene;
		bool requestStop;
		struct DEBUGSTUFF {std::string doing; unsigned int time; int id;};
		std::vector<DEBUGSTUFF> debugThreadStuff;
		TraceThread(TraceScene* owner) : scene(owner), requestStop(false) {}
		virtual void run();
	};
	struct Light {
		vec3f intensity;
		vec3f center;
		mat44 transform; //transforms samples
		std::vector<vec2f> samples;
		bool square;
	};
	friend struct TraceThread;
public:
	TraceScene();
	~TraceScene();
	
	struct HitInfo {
		vec3f pos;
		float s, t; //barycentric coords
		float time; //ratio along ray (for depth sorting)
		Vertex interp; //interpolated vertex
		Triangle* triangle;
		bool backface;
	};
	
	struct Ray {
		enum RayType {
			SHADOW        = 1<<1,
			GLOBAL        = 1<<2,
			TRANSPARENT   = 1<<3,
			REFRACT       = 1<<4,
			REFLECT       = 1<<5,
			
			DEBUG2        = 1<<6,
			DEBUG1        = 1<<7,
		};
		struct Diff {
			vec3f P, D;
			Diff() {}
			Diff(const vec3f& p, const vec3f& d) : P(p), D(d) {}
			Diff operator-(void) {return Diff(-P,-D);}
			Diff operator-(const Diff& d) {return Diff(P-d.P,D-d.D);}
			Diff operator+(const Diff& d) {return Diff(P+d.P,D+d.D);}
		};
		vec3f start;
		vec3f end;
		vec3f dir; //for convenience
		Diff d[4]; //ray differentials
		vec4f intensity;
		std::set<Triangle*> lastHit; //triangle index
		int depth;
		int canary;
		uchar mask; //RayType
		mat44 projection; //for EXTREME filtering :D
		/*
		Ray() {}
		void operator=(const Ray& o)
		{
			start = o.start;
			end = o.end;
			dir = o.dir;
			d[0] = o.d[0];
			d[1] = o.d[1];
			d[2] = o.d[2];
			d[3] = o.d[3];
			intensity = o.intensity;
			lastHit = o.lastHit;
			depth = o.depth;
			canary = o.canary;
			mask = o.mask;
			projection = o.projection;
		}
		Ray(const Ray& o)
		{
			start = o.start;
			end = o.end;
			dir = o.dir;
			d[0] = o.d[0];
			d[1] = o.d[1];
			d[2] = o.d[2];
			d[3] = o.d[3];
			intensity = o.intensity;
			lastHit = o.lastHit;
			depth = o.depth;
			canary = o.canary;
			mask = o.mask;
			projection = o.projection;
		}
		*/
		float transfer(const HitInfo& hitInfo, const vec3f& incidence); //returns distance to surface
		void reflect(const HitInfo& hitInfo, const vec3f& incidence, const vec3f (&curve)[4]);
		void refract(const HitInfo& hitInfo, const vec3f& incidence, const vec3f (&curve)[4], float eta);
		void tangentDiff(const TraceScene::HitInfo& surface, const Vertex& a, const Vertex& b, const Vertex& c, vec3f (&curve)[4], vec2f (&area)[4]);
	};
	
	struct DOF {
		int samples;
		float focus; //distance to stuff that should be "in focus" / "depth-of-field" center distance
		float aperture; //size of aperture in world coordinates
	} dof;
	
	struct GI {
		int samples;
		float totalDiffuse; //sum of samples.z
		float maxDistance;
		int maxDepth;
		std::vector<vec3f> samplesHemisphere; //get populated on render
	} gi;
	
	struct Photons {
		float maxDistance;
		int emit; //total number of photons to emit. photons may split (refract/reflect). not used if no refractive/reflective surfaces
		std::vector<vec3f> emitSphere; //for random direciton photon shooting. will be much less than "emit"
	} photons;
	
	struct Filtering
	{
		int anisotropic;
		float sqrtSamples;
		std::vector<vec2f> samples;
	} filtering;
	
	struct Gloss
	{
		int samples;
		float totalDiffuse;
		std::vector<vec3f> normalOffsets;
	} gloss;
	
	float traversalCost;
	float intersectCost;
	bool cullBackface;
	vec4f background;
	Material* defaultMaterial;
private:

	struct Photon
	{
		vec3f colour;
		vec3f dir;
		HitInfo hit;
		float radius;
	};
	
	typedef std::stack<Ray> TraceStack;

	bool shootPhotons;
	int totalJobs;
	int treeDepth;
	Mutex jobMutex;
	Bounds sceneBounds;
	std::vector<Photon> photonInfo;
	std::vector<vec2f> samplesDisc; //using this for dof
	std::vector<TraceThread*> threads;
	std::queue<TraceThreadJob> jobs;
	std::vector<Material*> materials;
	std::vector<Triangle> triangleData; //precomputed triangle info
	std::vector<Vertex> vertexData; //standard vertex attributes for interpolation
	std::vector<uint> triangles; //leaf data. indexes triangleData
	std::vector<Node> tree; //list of KD-Tree nodes, leaves point to triangle ranges
	std::vector<Bounds> triangleBounds; //bounds of each triangle within mesh
	std::vector<vec3f> debugTriangles;
	std::vector<vec3f> debugTriangles2;
	std::vector<vec3f> debugTriangles3;
	inline const vec2i textureRepeatClamp(const QI::Image* img, vec2i pos);
	vec4f texelFetch(QI::Image* img, vec2i pos); //pos must be in bounds! use textureRepeatClamp
	vec4f texture2D(MaterialTexture* texture, vec2f pos, int mipmap = 0);
	vec4f texture2D(MaterialTexture* texture, vec2f pos, const vec2f& dTdx, const vec2f& dTdy);
	vec4f texture2D(MaterialTexture* texture, vec2f pos, const vec2f (&d)[4]);
	inline Vertex interpolateVertex(int a, int b, int c, float s, float t);
	int intersectTriSquare(int axis, float pos, const vec2f& bmin, const vec2f& bmax, const vec3f& a, const vec3f& b, const vec3f& c);
	inline bool intersectRayTriangle(const Ray& ray, const Triangle& triangle, HitInfo& hit);
	Bounds intersectVoxel(const Bounds& a, const Bounds& b); //voxels are assumed to be overlapping
	void clipTriangleBounds(SAHTriangle t, SAHSplit split, Bounds& left, Bounds& right);
	float SA(Bounds voxel);
	float SAH(float Pl, float Pr, int Nl, int Nr);
	void SAH(Bounds v, SAHSplit& split, int Nl, int Nr, int Np);
	void addEvents(std::vector<SAHEvent>& E, const SAHTriangle& t);
	void findSplit(Bounds voxel, std::vector<SAHEvent>& events, int totalTriangles, SAHSplit& bestSplit);
	void doSplit(SAHSplit split, const std::vector<SAHTriangle>& T, const std::vector<SAHEvent>& E, std::vector<SAHTriangle>& Tl, std::vector<SAHEvent>& El, std::vector<SAHTriangle>& Tr, std::vector<SAHEvent>& Er);
	int rbuild(int depth, std::vector<SAHTriangle>& T, std::vector<SAHEvent>& E, Bounds voxel);
	
	enum TraceFlags {
		TRACE_CAMERA  = 1 << 0,
		TRACE_SHADOW  = 1 << 1,
		TRACE_PHOTON = 1 << 2,
		TRACE_DEBUG   = 1 << 3,
	};
	
	bool hitSurface(vec4f& colour, Ray& ray, HitInfo& hitInfo, TraceStack& rays, int sampleOffset, int traceFlags); //return true to stop tracing along the current ray
	bool trace(vec4f& colour, Ray& ray, HitInfo& hitInfo, TraceStack& rays, int sampleOffset, int traceFlags); //returns true if one or more surfaces were hit
	bool trace(vec4f& colour, TraceStack& rays, int sampleOffset, int traceFlags); //trace until TraceStack is empty
	void performJob(TraceThreadJob& job);
	
	//no copying!
	TraceScene(const Thread& other) {}
	TraceScene& operator=(const Thread& other) {return *this;}
	
protected:
	struct RenderInfo
	{
		QI::Image* image;
		Camera* camera;
		int nthreads;
		bool initialized; //main render thread initialized
		bool initializingThreads; //after photon tracing, the main render threads start, turning this off
		Condvar initBarrier; //to notify parent thread initialized is true and the threadMutex has been entered
		Mutex initMutex; //locks the initialized boolean
		Mutex threadMutex; //for starting, stopping and waiting child threads
	} renderInfo;
	virtual void run();
	
public:

	//FIXME: make private
	RTree* photonTree;
	
	float shadowScale; //deprecated: not physically based but need it for paper
	
	bool debug;
	Mutex debugMutex;
	std::vector<vec3f> debugPoints;
	std::vector<Ray> debugRays;
	std::vector<Light> lights;
	std::vector<vec3f> photonPoints;
	VBOMesh* debugMesh; //draws split planes
	VBOMesh* debugMeshTrace; //draws trace triangles
	VBOMesh* debugMeshTrace2; //draws trace triangles
	VBOMesh* debugMeshTrace3; //draws trace triangles
	void addLight(mat44 transform, vec3f intensity, int samples = 1, float radius = 0.0f, bool square = false);
	void addMesh(VBOMesh* mesh, mat44 transform = mat44::identity());
	void build();
	//bool traceFirstHit(vec3f start, vec3f end, HitInfo& hitInfo); //single segment first-intersection test
	
	void traceCameraRay(vec3f start, vec3f end, vec4f& colour, int sampleOffset = 0, bool debugTrace = false); //the expensive, recursive one
	void traceCameraRay(vec3f start, vec3f end, Ray::Diff dx, Ray::Diff dy, vec4f& colour, int sampleOffset = 0, bool debugTrace = false);
	void tracePhotons();
	void cancel(); //stops threads. blocks!
	void wait(); //waits until render finishes
	void render(QI::Image* image, Camera* camera, int nthreads);
	float getProgress();
	void test();
};

#endif

