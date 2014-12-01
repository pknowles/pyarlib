/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

/*
Visual studio *spit* *spit* *spit* steps:
1. Add entire pyarlib directory to a pyarlib library project or your main executable
2. Add resources.rc to your main executable
3. Include these libraries (project properties->linker->input->additional dependecies):
	glew32.lib;freetype248.lib;SDL2.lib;libpng.lib;zlib.lib;opengl32.lib;glu32.lib
4. Some files (currently just "pyarlib/mesh/simpleobj/obj.c") need to be compiled as C++
	project properties->C++->advanced->compile as
5. Build (this file, template.txt, is an example main.cpp file)
	the following include might need " quotes and a local path instead
*/

#include <pyarlib/pyarlib.h>

Jeltz jeltz("Jeltz");
JeltzFly fly;
JeltzGUI gui;

VBOMesh sphere;

void update(float dt)
{
	if (jeltz.buttonDown("`"))
	{
		gui.visible = !gui.visible;
		gui.fps.print = !gui.visible;
	}
	
	static float reloadTimer = 0.0f;
	reloadTimer -= dt;
	if (reloadTimer < 0.0f)
	{
		reloadTimer = 1.0f;
		if (Shader::reloadModified())
			jeltz.postUnfocusedRedisplay();
	}
}

void display()
{
	fly.uploadCamera();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	sphere.draw();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

int main()
{
	jeltz.setUpdate(update);
	jeltz.setDisplay(display);
	jeltz.add(&gui);
	jeltz.add(&fly);
	jeltz.init();

	Material::defaultAnisotropy = 16;
	VBOMeshOBJ::registerLoader();
	VBOMesh3DS::registerLoader();
	VBOMeshCTM::registerLoader();
	VBOMeshIFS::registerLoader();
	FileFinder::addDir(Config::getString("models"));

	sphere = VBOMesh::grid(vec2i(64, 32), VBOMesh::paramSphere);
	sphere.upload();

	jeltz.run();
	return 0;
}
