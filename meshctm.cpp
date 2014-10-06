/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

#include <openctm/openctm.h>

#include "vbomesh.h"
#include "meshctm.h"
#include "material.h"

bool VBOMeshCTM::smoothGeneratedNormals = false;
bool VBOMeshCTM::registerLoader()
{
	return VBOMesh::registerLoader(".ctm", load);
}

bool VBOMeshCTM::load(VBOMesh& mesh, const char* filename)
{
	mesh.release();
	mesh.error = false;
	
	//http://openctm.sourceforge.net/media/DevelopersManual.pdf
	CTMcontext context;

	// Create a new importer context
	context = ctmNewContext(CTM_IMPORT);

	// Load the OpenCTM file
	ctmLoad(context, filename);
	CTMenum err = ctmGetError(context);
	if(err != CTM_NONE)
	{
		printf("Error opening %s\n", filename);
		ctmFreeContext(context);
		mesh.error = true;
		return false;
	}

	// Access the mesh data
	mesh.numVertices = ctmGetInteger(context, CTM_VERTEX_COUNT);
	int norms = ctmGetInteger(context, CTM_HAS_NORMALS);
	int texcs = ctmGetInteger(context, CTM_UV_MAP_COUNT);
	int other = ctmGetInteger(context, CTM_ATTRIB_MAP_COUNT);
	mesh.numPolygons = ctmGetInteger(context, CTM_TRIANGLE_COUNT);
	mesh.numIndices = 3 * mesh.numPolygons;

	mesh.sub[VERTICES] = (float*)ctmGetFloatArray(context, CTM_VERTICES);
	if (norms)
		mesh.sub[NORMALS] = (float*)ctmGetFloatArray(context, CTM_NORMALS);
	if (texcs)
		mesh.sub[TEXCOORDS] = (float*)ctmGetFloatArray(context, CTM_UV_MAP_1);
	if (other)
		mesh.sub[TANGENTS] = (float*)ctmGetFloatArray(context, CTM_ATTRIB_MAP_1);
		
	delete[] mesh.dataIndices;
	mesh.dataIndices = new unsigned int[mesh.numIndices];
	mesh.indexed = true;

	memcpy(mesh.dataIndices, ctmGetIntegerArray(context, CTM_INDICES), mesh.numIndices * sizeof(unsigned int));
	
	if (!norms)
	{
		printf("Generating normals\n");
		
		float* tmp = NULL;
		if (smoothGeneratedNormals)
		{
			//generate normals as though vertices were smoothed a little
			tmp = mesh.sub[VERTICES];
			mesh.sub[VERTICES] = new float[mesh.numVertices*3];
			memcpy(mesh.sub[VERTICES], tmp, mesh.numVertices*3*sizeof(float));
			
			mesh.averageVertices();
		}
		
		mesh.generateNormals();
		
		if (tmp)
		{
			//free tmp data - use actual vertices
			delete[] mesh.sub[VERTICES];
			mesh.sub[VERTICES] = tmp;
		}
	}

	mesh.interleave(false);
	
	if (!mesh.validate())
		printf("FAILED TO VALIDATE\n");

	//important - remove pointers. we didn't allocate that data
	mesh.sub[VERTICES] = NULL;
	mesh.sub[NORMALS] = NULL;
	mesh.sub[TEXCOORDS] = NULL;
	mesh.sub[TANGENTS] = NULL;

	// Free the context
	ctmFreeContext(context);
	
	#if 0
	//check for a material file
	int lastIndex = -1;
	Material* m = NULL;
	std::string currentMatName;
	std::string matFilename = basefilename(filename) + ".mctm";
	std::ifstream matfile(matFilename.c_str());
	if (matfile.is_open())
	{
		std::string line;
		while (std::getline(matfile, line))
		{
			std::stringstream r(line);
			std::string stuff;
			r >> stuff;
			if (stuff[0] == '#')
				continue;
				
			//m->imgColour.load(obj->materials[i].texture);
			
			if (stuff == "newmtl")
			{
				r >> stuff;
				MaterialCache::getMaterial(filename + stuff, m);
				mesh.addMaterial(m, stuff);
			}
			else if (m && stuff == "Kd")
			{
				r >> m->colour.x;
				r >> m->colour.y;
				r >> m->colour.z;
				if (!(r >> m->colour.w))
					m->colour.w = 1.0f;
			}
			else if (stuff == "m")
			{
				int triangle;
				r >> stuff;
				r >> triangle; //this reads the face index. useless
				r >> triangle; //triangle index. since file should be triangulated
				int triangleStartIndex = triangle * 3;
				if (lastIndex >= 0 && triangleStartIndex - lastIndex > 0 && currentMatName.size())
				{
					std::cout << lastIndex << " " << triangleStartIndex << " " << currentMatName << std::endl;
					mesh.useMaterial(lastIndex, triangleStartIndex, currentMatName);
				}
				currentMatName = stuff;
				lastIndex = triangleStartIndex;
			}
		}
		
		if (currentMatName.size())
		{
			std::cout << lastIndex << " " << mesh.numIndices << " " << currentMatName << std::endl;
			mesh.useMaterial(lastIndex, mesh.numIndices, currentMatName);
		}
	}
	#endif
	
	return true;

}
