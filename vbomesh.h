/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef VBOMESH_H
#define VBOMESH_H

#include "matrix.h"
#include "gpu.h"
#include "loader.h"

template<typename T>
struct InterleavedEditor
{
	int offset, stride;
	void* data;
	InterleavedEditor() {data=NULL;offset=0;stride=sizeof(T);}
	InterleavedEditor(int offset, int stride, void* data) : offset(offset), stride(stride), data(data) {}
	T& operator[](intptr_t index) {return *(T*)((char*)data+(offset+stride*index));}
	operator bool() const {return data != NULL;}
};

struct VBOMeshFaceset
{
	int startIndex;
	int endIndex;
	int material;
};

struct BindableMaterial;

struct VBOMesh : public Loader<VBOMesh>
{
	enum VertexDataType
	{
		VERTICES,
		NORMALS,
		TEXCOORDS,
		TANGENTS,
		DATA_TYPES
	};

	static const int dataTypes = DATA_TYPES;

	static const int size[dataTypes];

	bool indexed; //drawArrays/drawElements
	bool error;
	bool buffered;
	bool interleaved; //cannot be draw()n without interleave()ing
	GLenum primitives;

	int offset[dataTypes];
	bool has[dataTypes];
	float* sub[dataTypes];

	int numVertices;
	int numIndices;
	int numPolygons; //for stats only
	int stride; //in bytes
	int strideFloats; //in floats

	VertexBuffer vertices;
	IndexBuffer indices;
	float* data;
	unsigned int* dataIndices;
	
	typedef std::map<int, VBOMeshFaceset> Facesets;
	Facesets facesets;
	std::vector<BindableMaterial*> materials;
	std::map<std::string, int> materialNames;

	GLuint vloc, nloc, txloc, tgloc;
	
	vec3f boundsMin, average, center, boundsMax, boundsSize; //call computeInfo() to populate

	VBOMesh();
	virtual ~VBOMesh();
	
	//if no args provided, will use old glEnableClientState method
	void setMaterial(BindableMaterial* material); //shortcut to add a single material
	void useMaterial(int start, int end, std::string name);
	void addMaterial(BindableMaterial* material, std::string name); //FIXME: this passes on ownership of the data. don't delete it!
	void bind(GLuint vloc = -1, GLuint nloc = -1, GLuint txloc = -1, GLuint tgloc = -1);
	void draw(int instances = 1, bool autoAttribLocs = true); //if not uploaded, will call upload(true).
	void upload(bool freeLocal = true);
	void allocate(); //called during interleave to allocate float* data
	void calcInternal();
	void interleave(bool freeSource = true);
	void uninterleave(bool freeSource = true);
	bool computeInfo();
	void transform(const mat44& m);
	bool validate();
	bool triangulate();
	void invertNormals();
	void repairWinding();
	void normalize(bool onground = false); //scales to cover unit size and centers. onground moves up to ground plane
	
	template <typename T> InterleavedEditor<T> getAttrib(VertexDataType attr)
	{
		InterleavedEditor<T> ret;
		if (!has[attr])
			return ret;
		assert(interleaved || sub[attr]);
		if (sub[attr])
		{
			ret.data = sub[attr];
		}
		else
		{
			ret.data = data;
			ret.offset = offset[attr] * sizeof(float);
			ret.stride = stride;
		}
		return ret;
	}

	void averageVertices();
	void generateNormals();
	void generateTangents();
	void realloc(bool verts, bool norms, bool texcs, bool tangents);
	bool inject(float* data, bool verts = true, bool norms = false, bool texcs = false, bool tangents = false);
	bool release();
	int getStride();
	
	void writeOBJ(std::string filename);

	static void paramPlane(const vec2f& point, vec3f& vertex, vec3f& normal, vec2f& texcoord, vec3f& tangent);
	static void paramSphere(const vec2f& point, vec3f& vertex, vec3f& normal, vec2f& texcoord, vec3f& tangent);
	
	static int generateTriangleStripGrid(vec2i tess, unsigned int*& indices);
	static int generateTrianglesGrid(vec2i tess, unsigned int*& indices);

	static VBOMesh grid(vec2i size, vec3f (*param)(const vec2f&)); //param returns vert
	static VBOMesh grid(vec2i size, void (*param)(const vec2f&, vec3f&)); //out args vert
	static VBOMesh grid(vec2i size, void (*param)(const vec2f&, vec3f&, vec3f&)); //out args vert, norm
	static VBOMesh grid(vec2i size, void (*param)(const vec2f&, vec3f&, vec3f&, vec2f&)); //out args vert, norm, tex
	static VBOMesh grid(vec2i size, void (*param)(const vec2f&, vec3f&, vec3f&, vec3f&)); //out args vert, norm, tangent
	static VBOMesh grid(vec2i size, void (*param)(const vec2f&, vec3f&, vec3f&, vec2f&, vec3f&) = paramPlane); //out args vert, norm, tex, tangent
	static VBOMesh cube(float size = 1.0f);
	static VBOMesh cubeWire(float size = 1.0f);
	static VBOMesh quad();
};

#endif
