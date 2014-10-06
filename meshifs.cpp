/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

#ifdef _WIN32
#include <stdlib.h>
#define BYTESWAP32(X) _byteswap_ulong(X)
#else
#include <byteswap.h>
#define BYTESWAP32(X) __bswap_32(X)
#endif

#include <simpleobj/obj.h>

#include "vbomesh.h"
#include "material.h"
#include "imgpng.h"
#include "fileutil.h"

#include "meshifs.h"

using namespace QI;
using namespace std;

static bool getInteger(ifstream& f, unsigned int& i)
{
	f.read((char*)&i, sizeof(unsigned int));
	return !f.fail();
}

static bool getFloat(ifstream& f, float& x)
{
	f.read((char*)&x, sizeof(float));
	return !f.fail();
}

static bool getString(ifstream& f, string& s)
{
	unsigned int l;
	if (!getInteger(f, l))
		return false;
	if (l == 0 || l > 8*1024) //fairly arbitrary
	{
		s.clear();
		return false;
	}
	s.resize(l-1);
	f.read(&s[0], l);
	return !f.fail();
}

bool VBOMeshIFS::registerLoader()
{
	return VBOMesh::registerLoader(".ifs", load);
}

bool VBOMeshIFS::load(VBOMesh& mesh, const char* filename)
{
	mesh.release();
	mesh.error = false;
	
	ifstream ifile(filename, ios::binary);
	if (!ifile.is_open())
	{
		mesh.error = true;
		printf("Error opening %s\n", filename);
		return false;
	}
	
	float version;
	unsigned int numVertices, numFaces;
	string header, name, headerVertices, headerTriangles;
	
	//read header
	mesh.error = mesh.error || !getString(ifile, header);
	mesh.error = mesh.error || !getFloat(ifile, version);
	if (mesh.error || header != "IFS" || version != 1.0f)
	{
		printf("Error parsing %s (version:%f)\n", filename, version);
		return false;
	}
	mesh.error = mesh.error || !getString(ifile, name);
	
	//read vertices
	mesh.error = mesh.error || !getString(ifile, headerVertices);
	mesh.error = mesh.error || !getInteger(ifile, numVertices);
	if (mesh.error || headerVertices != "VERTICES")
	{
		printf("Error parsing %s (version:%f)\n", filename, version);
		return false;
	}
	mesh.numVertices = numVertices;
	mesh.sub[VERTICES] = new float[mesh.numVertices * 3];
	ifile.read((char*)mesh.sub[VERTICES], mesh.numVertices * 3 * sizeof(float));
	
	//read indices
	mesh.error = mesh.error || !getString(ifile, headerTriangles);
	mesh.error = mesh.error || !getInteger(ifile, numFaces);
	if (mesh.error || headerTriangles != "TRIANGLES")
	{
		mesh.numVertices = 0;
		delete[] mesh.sub[VERTICES];
		mesh.sub[VERTICES] = NULL;
		printf("Error parsing %s (version:%f)\n", filename, version);
		return false;
	}
	mesh.numIndices = numFaces * 3;
	mesh.numPolygons = numFaces;
	mesh.dataIndices = new unsigned int[mesh.numIndices];
	ifile.read((char*)mesh.dataIndices, mesh.numIndices * sizeof(unsigned int));
	
	//flip indices endianness if this isn't littleEndian
	if (isBigEndian())
		for (unsigned int i = 0; i < numFaces*3; ++i)
			BYTESWAP32(mesh.dataIndices[i]);
	
	//populate mesh attribs
	mesh.has[VERTICES] = true;
	mesh.has[NORMALS] = false;
	mesh.has[TEXCOORDS] = false;
	mesh.has[TANGENTS] = false;
	mesh.interleaved = false;
	mesh.indexed = true;
	mesh.calcInternal();
	mesh.primitives = GL_TRIANGLES;
	
	//generate normals - file format doesn't have any
	mesh.generateNormals();
	return true;

}
