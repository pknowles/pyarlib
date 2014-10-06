/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

#include "mesh/simpleobj/obj.h"
#include "vbomesh.h"
#include "material.h"
#include "imgpng.h"
#include "fileutil.h"

#include "meshobj.h"

using namespace QI;

bool VBOMeshOBJ::registerLoader()
{
	return VBOMesh::registerLoader(".obj", load);
}
bool VBOMeshOBJ::load(VBOMesh& mesh, const char* filename)
{
	mesh.release();
	mesh.error = false;
	
	std::string path = basefilepath(filename);
	
	OBJMesh* obj = objMeshLoad(filename);
	if (!obj)
	{
		mesh.error = true;
		printf("Error opening %s\n", filename);
		return false;
	}
	
	//printf("v%i t%c n%c i%i m%i\n", obj->numVertices, obj->hasTexCoords?'t':'f', obj->hasNormals?'t':'f', obj->numIndices, obj->numFacesets);
	
	//don't copy. it's already in the right format
	//FIXME: this data uses malloc() and will get deleted later with delete. doesn't seem to give issues but is probably quite bad
	/*
	mesh.dataIndices = obj->indices;
	mesh.data = obj->vertices;
	*/
	mesh.dataIndices = new unsigned int[obj->numIndices];
	memcpy(mesh.dataIndices, obj->indices, obj->numIndices * sizeof(unsigned int));
	mesh.data = new float[obj->numVertices * obj->stride / 4];
	memcpy(mesh.data, obj->vertices, obj->numVertices * obj->stride);
	mesh.numVertices = obj->numVertices;
	mesh.numIndices = obj->numIndices;
	mesh.numPolygons = mesh.numIndices / 3;
	
	for (int i = 0; i < obj->numMaterials; ++i)
	{
		//printf("%s %s %s %s\n", filename, obj->materials[i].texture, obj->materials[i].texNormal, obj->materials[i].texSpecular);
		Material* m;
		if (!MaterialCache::getMaterial(std::string(filename) + obj->materials[i].name, m))
		{
			memcpy(&m->colour, &obj->materials[i].diffuse[0], sizeof(float)*4);
			memcpy(&m->ambient, &obj->materials[i].ambient[0], sizeof(float)*3);
			memcpy(&m->specular, &obj->materials[i].specular[0], sizeof(float)*3);
			m->shininess = obj->materials[i].shininess;
			if (obj->materials[i].texture)
				m->imgColour.load(obj->materials[i].texture);
			if (obj->materials[i].texNormal)
				m->imgNormal.load(obj->materials[i].texNormal);
			if (obj->materials[i].texSpecular)
				m->imgSpecular.load(obj->materials[i].texSpecular);
		}
		mesh.addMaterial(m, obj->materials[i].name);
	}
	
	for (int i = 0; i < obj->numFacesets; ++i)
	{
		//printf("%i %i->%i %s\n", i, obj->facesets[i].indexStart, obj->facesets[i].indexEnd, obj->materials[obj->facesets[i].material].name);
		if (obj->facesets[i].material >= 0)
			mesh.useMaterial(obj->facesets[i].indexStart, obj->facesets[i].indexEnd, obj->materials[obj->facesets[i].material].name);
	}
	
	/*
	for (int i = 0; i < obj->numFacesets; ++i)
		printf("%i %i->%i m%i(%s) s%i\n", i, obj->facesets[i].indexStart, obj->facesets[i].indexEnd,
			obj->facesets[i].material,
			obj->facesets[i].material >= 0 ? obj->materials[obj->facesets[i].material].name : "<none>",
			obj->facesets[i].smooth);
	*/
	
	/*
	obj->indices = NULL;
	obj->vertices = NULL;
	obj->numIndices = 0;
	obj->numVertices = 0;
	//... data stolen
	//yes it's bad but I wrote both libs, and there's no one around to yell at me
	*/
	
	mesh.has[VERTICES] = true;
	mesh.has[NORMALS] = obj->hasNormals != 0;
	mesh.has[TEXCOORDS] = obj->hasTexCoords != 0;
	mesh.has[TANGENTS] = false;
	mesh.interleaved = true;
	mesh.indexed = true;
	mesh.calcInternal();
	mesh.primitives = GL_TRIANGLES;
	
	//since pointers were set to NULL, this is ok
	objMeshFree(&obj);
	
	if (!mesh.has[NORMALS])
	{
		printf("Generating normals (meshobj)\n");
		mesh.uninterleave();
		mesh.generateNormals();
	}
	return true;

}
