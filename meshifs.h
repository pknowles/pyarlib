/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef VBOMESH_IFS_H
#define VBOMESH_IFS_H

#include "vbomesh.h"

class VBOMeshIFS : VBOMesh
{
public:
	static bool registerLoader();
	static bool load(VBOMesh& mesh, const char* filename);
};

#endif
