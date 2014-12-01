
#include "prec.h"
#include "scene.h"
#include "camera.h"
#include "config.h"
#include "quickgui.h"
#include "meshobj.h"
#include "jeltz.h"
#include "util.h"
#include "material.h"
#include "meshctm.h"
#include "meshobj.h"
#include "mesh3ds.h"
#include "meshifs.h"
#include "trace.h"
#include "imgpng.h"

#include "pugixml.h"

#include <assert.h>

#define GENERATE_TANGENTS 1

using namespace std;

namespace pyarlib
{

static Shader widgetShader("pyarlib-scene-tool");

static VBOMesh* sphere;
vec3f debug;
		
void Scene::CamKey::toXML(pugi::xml_node* node)
{
	std::stringstream pos, rot;
	pos << position.x << " " << position.y << " " << position.z;
	rot << rotation.x << " " << rotation.y << " " << rotation.z << " " << rotation.w;
	node->append_attribute("position") = pos.str().c_str();
	node->append_attribute("rotation") = rot.str().c_str();
	node->append_attribute("aperture") = aperture;
	node->append_attribute("focus") = focus;
	node->append_attribute("speed") = speed;
	node->append_attribute("interp") = interp;
}
void Scene::CamKey::fromXML(pugi::xml_node* node)
{
	stringstream(node->attribute("position").value()) >> position.x >> position.y >> position.z;
	stringstream(node->attribute("rotation").value()) >> rotation.x >> rotation.y >> rotation.z >> rotation.w;
	aperture = node->attribute("aperture").as_float();
	focus = node->attribute("focus").as_float();
	speed = node->attribute("speed").as_float();
	interp = node->attribute("interp").as_int();
}

void Scene::_changeFlythrough(QG::Widget* w, void* dat)
{
	static_cast<Scene*>(dat)->setFlythrough(dynamic_cast<QG::Slider*>(w)->i);
}
void Scene::_addModel(void* dat)
{
	NewObjectDat* d = (NewObjectDat*)dat;
	d->owner->add(d->name);
}
void Scene::_changeKey(QG::Widget* w, void* dat)
{
	static_cast<Scene*>(dat)->jumpFlythrough(dynamic_cast<QG::Slider*>(w)->i);
}
void Scene::_changeRatio(QG::Widget* w, void* dat)
{
	static_cast<Scene*>(dat)->jumpFlythrough(dynamic_cast<QG::Slider*>(w)->f);
}
void Scene::_addKey(void* dat)
{
	static_cast<Scene*>(dat)->insertKeyframe();
}
void Scene::_repKey(void* dat)
{
	static_cast<Scene*>(dat)->setKeyframe();
}
void Scene::_delKey(void* dat)
{
	static_cast<Scene*>(dat)->removeKeyframe();
}
void Scene::_saveFlythrough(void* dat)
{
	Scene* scene = static_cast<Scene*>(dat);
	scene->saveFlythrough(scene->currentFlythrough);
}

Scene::Scene()
{
	forceDoubleSided = false;
	useLights = true;
	editActive = false;
	editMode = false;
	editTool = 0;
	editObject = -1;
	editAxis = -1;
	currentFlythrough = -1;
	flythroughKey = 0;
	playing = false;
	splineIntegrateSteps = 200;
	editTransform = mat44::identity();
	controls = new QG::Widget;
	controls->hide(); //start hidden until edit mode is enabled
	controls->add(guiElements["add"] = new QG::DropDown("Add Model"));
	controls->add(guiElements["flythrough"] = new QG::Slider("Flythrough", 0, 0));
	controls->add(guiElements["frames"] = new QG::Slider("Keyframes", 0, 0));
	controls->add(guiElements["ratio"] = new QG::Slider("Position", 0, 1, true));
	controls->add(guiElements["addkey"] = new QG::Button("Insert Key"));
	controls->add(guiElements["repkey"] = new QG::Button("Replace Key"));
	controls->add(guiElements["delkey"] = new QG::Button("Delete Key"));
	controls->add(guiElements["save"] = new QG::Button("Save"));
	guiElements["flythrough"]->capture(QG::SCROLL, Scene::_changeFlythrough, this);
	guiElements["frames"]->capture(QG::SCROLL, Scene::_changeKey, this);
	guiElements["ratio"]->capture(QG::SCROLL, Scene::_changeRatio, this);
	guiElements["addkey"]->capture(QG::CLICK, Scene::_addKey, this);
	guiElements["repkey"]->capture(QG::CLICK, Scene::_repKey, this);
	guiElements["delkey"]->capture(QG::CLICK, Scene::_delKey, this);
	guiElements["save"]->capture(QG::CLICK, Scene::_saveFlythrough, this);
	guiElements["ratio"]->width = 200;
	
	//debug sphere
	sphere = new VBOMesh();
	*sphere = VBOMesh::grid(vec2i(32), VBOMesh::paramSphere);
	sphere->transform(mat44::scale(0.01f));
	
	globalScale = 1.0f;
	
	widget[0] = NULL;
	widget[1] = NULL;
	widget[2] = NULL;
	cameraIcon = NULL;
	
	toolmat[0] = mat44::identity();
	toolmat[1] = mat44::rotate(pi*0.5f,vec3f(0,0,1));
	toolmat[2] = mat44::rotate(pi*0.5f,vec3f(0,-1,0));
	for (int i = 0; i < 3; ++i)
		toolmatInv[i] = toolmat[i].inverse();
}
Scene::~Scene()
{
	delete controls;
	//widget[0]->release();
	delete widget[0];
	//widget[1]->release();
	delete widget[1];
	//widget[2]->release();
	delete widget[2];
	
	for (Meshes::iterator it = meshes.begin(); it != meshes.end(); ++it)
	{
		//delete it->second;
	}
	meshes.clear();
	objects.clear();
}
int Scene::getNumFlythroughs()
{
	return flythroughs.size();
}
void Scene::setFlythrough(int i)
{
	if (i < 0 || i >= (int)flythroughs.size())
	{
		printf("Error: Scene has %i flythroughs. Cannot set %i\n", (int)flythroughs.size(), i);
		return;
	}
	currentFlythrough = i;
	jumpFlythrough(0, 0.0f);
	dynamic_cast<QG::Slider*>(guiElements["flythrough"])->i = currentFlythrough;
	dynamic_cast<QG::Slider*>(guiElements["flythrough"])->setDirty();
	dynamic_cast<QG::Slider*>(guiElements["frames"])->i = flythroughKey;
	dynamic_cast<QG::Slider*>(guiElements["frames"])->upper = (int)flythroughs[currentFlythrough].keys.size()-1;
	dynamic_cast<QG::Slider*>(guiElements["frames"])->setDirty();
}
void Scene::playFlythrough(int i)
{
	if (i < 0 || i >= (int)flythroughs.size())
	{
		printf("Error: Scene has %i flythroughs. Cannot play %i\n", (int)flythroughs.size(), i);
		return;
	}
	setFlythrough(i);
	playing = true;
}
void Scene::stopFlythrough()
{
	playing = false;
}
void Scene::jumpFlythrough(int keyframe, float ratio)
{
	if (currentFlythrough < 0 || currentFlythrough >= (int)flythroughs.size())
		return;
	Flythrough& f = flythroughs[currentFlythrough];
	if ((int)f.keys.size() < 2)
		return;
	if (keyframe < 0 || keyframe >= (int)f.keys.size())
		printf("Warning: jumpFlythrough keyframe %i out of bounds\n", keyframe);
	flythroughKey = keyframe = myclamp(keyframe, 0, (int)f.keys.size()-1);
	dynamic_cast<QG::Slider*>(guiElements["frames"])->i = flythroughKey;
	dynamic_cast<QG::Slider*>(guiElements["frames"])->setDirty();
	
	//update position
	int i = keyframe + 1;
	if (i >= (int)f.keys.size())
	{
		i = (int)f.keys.size()-1;
		ratio = 1.0f;
	}
	vec3f a = f.keys[i-1].position;
	vec3f b = f.keys[i].position;
	vec3f aDir = i > 1 ? b - f.keys[i-2].position : b - a;
	vec3f bDir = i < (int)f.keys.size()-1 ? f.keys[i+1].position - a : b - a;

	//calculate hermite interp for position
	float keydist = f.keys[i-1].distance;
	float rtest = 0.0f;
	float x = 0.0f;
	vec3f lastPos = a;
	vec3f pos;
	if (f.keys[i-1].interp == 0 && keydist > 0.000001f)
	{
		for (int j = 1; j < splineIntegrateSteps; ++j)
		{
			x = j / (float)(splineIntegrateSteps-1);
			vec3f tmpPos = interpHermite(a, aDir * 0.5, b, bDir * 0.5, x);
			float delta = (tmpPos - lastPos).size();
			lastPos = tmpPos;
			rtest += delta / keydist;
			if (rtest >= ratio)
			{
				x *= ratio / rtest;
				break;
			}
		}
		pos = interpHermite(a, aDir * 0.5, b, bDir * 0.5, x);
	}
	else
		pos = b;
		
	//update flythroughtime
	flythroughTime = f.keys[keyframe].startTime + ratio * f.keys[keyframe].time;
		
	//set position
	camera->setPosition(pos);
	camera->setZoom(0.0f);
	
	//interp atributes
	camera->aperture = interpLinear(f.keys[i-1].aperture, f.keys[i].aperture, ratio);
	camera->focus = interpLinear(f.keys[i-1].focus, f.keys[i].focus, ratio);
	
	//interp rotation
	float r = interpCosine(0.0f, 1.0f, ratio);
	Quat rot = f.keys[i-1].rotation.slerp(f.keys[i].rotation, r);
	camera->setRotation(rot);
	camera->regenCamera();
}
void Scene::jumpFlythrough(float ratio)
{
	if (currentFlythrough < 0 || currentFlythrough >= (int)flythroughs.size())
		return;
	Flythrough& f = flythroughs[currentFlythrough];
	if (!f.keys.size())
		return;
	int k = 0;
	float d = ratio * f.time;
	while (k < (int)f.keys.size() && d >= f.keys[k].time)
		d -= f.keys[k++].time;
	k = mymin(k, (int)f.keys.size()-1);
	float r = f.keys[k].time > 0.0f ? d / f.keys[k].time : 0.0f;
	jumpFlythrough(k, r);
}
bool Scene::moveFlythrough(float time)
{
	if (currentFlythrough < 0 || currentFlythrough >= (int)flythroughs.size())
		return false;
	Flythrough& f = flythroughs[currentFlythrough];
	if (flythroughKey >= (int)f.keys.size() - 1)
	{
		playing = false;
		return false;
	}
	
	flythroughTime += time;
	jumpFlythrough(flythroughTime/flythroughs[currentFlythrough].time); //DIRTY HORRIBLE STIPUD WASTE OF PERFORMANCE. FIXME!!!!
	return true;
}
void Scene::setKeyframe()
{
	if (currentFlythrough < 0 || currentFlythrough >= (int)flythroughs.size())
		return;
	Flythrough& f = flythroughs[currentFlythrough];
	if (flythroughKey < 0 || flythroughKey >= (int)f.keys.size())
		return;
	printf("Setting key %i\n", flythroughKey);
	CamKey& key = f.keys[flythroughKey];
	key.position = camera->getZoomPos();
	key.rotation = camera->getRotation();
	key.aperture = camera->aperture;
	key.focus = camera->focus;
	key.distance = 0.0f;
	key.speed = 1.0f;
	key.time = 1.0f;
	
	computeFlythroughDistance(currentFlythrough, flythroughKey);
}
void Scene::computeFlythroughDistance(int flythrough, int keyframe)
{
	if (flythrough < 0 || flythrough >= (int)flythroughs.size())
		return;
	Flythrough& f = flythroughs[flythrough];
	
	int kfrom = keyframe >= 0 ? mymax(1, keyframe-1) : 1;
	int kto = keyframe >= 0 ? mymin(keyframe+3, (int)f.keys.size()) : (int)f.keys.size();
		
	//update all affected distances
	for (int i = kfrom; i < kto; ++i)
	{
		vec3f a = f.keys[i-1].position;
		vec3f b = f.keys[i].position;
		vec3f aDir = i > 1 ? b - f.keys[i-2].position : b - a;
		vec3f bDir = i < (int)f.keys.size()-1 ? f.keys[i+1].position - a : b - a;
		float d = 0.0f;
		vec3f last = a;
		for (int j = 1; j < splineIntegrateSteps; ++j)
		{
			vec3f v = interpHermite(a, aDir * 0.5, b, bDir * 0.5, j / (float)(splineIntegrateSteps-1));
			d += (v - last).size();
			last = v;
		}
		f.keys[i-1].distance = d;
		f.keys[i-1].time = mymax(2.0f, d / f.keys[i-1].speed);
		//printf("spline segment %i is %.2f long\n", i-1, f.keys[i-1].distance);
	}
	f.keys[f.keys.size()-1].time = 0.0f;
	f.keys[f.keys.size()-1].distance = 0.0f; //last fencepost has no distance
	f.distance = 0.0f;
	f.time = 0.0f;
	for (int i = 0; i < (int)f.keys.size() - 1; ++i) //last key doesn't store any segment information. it's a fence post
	{
		f.keys[i].startTime = f.time;
		f.distance += f.keys[i].distance;
		f.time += f.keys[i].time;
	}
		
	//update transforms. this function is as good as any but could separate
	kfrom = keyframe >= 0 ? keyframe : 0;
	kto = keyframe >= 0 ? keyframe+1 : (int)f.keys.size();
	for (int i = kfrom; i < kto; ++i)
		f.keys[i].transform = mat44::translate(f.keys[i].position) * f.keys[i].rotation.getMatrix() * mat44::scale(0.05f);
}
void Scene::insertKeyframe()
{
	if (currentFlythrough < 0 || currentFlythrough >= (int)flythroughs.size())
	{
		currentFlythrough = 0;
		if (!flythroughs.size())
		{
			flythroughs.push_back(Flythrough());
			dynamic_cast<QG::Slider*>(guiElements["flythrough"])->upper = flythroughs.size() - 1;
		}
	}
	
	printf("Inserting After %i\n", flythroughKey);
	
	Flythrough& f = flythroughs[currentFlythrough];
	f.keys.insert(f.keys.begin() + (f.keys.size()?++flythroughKey:0), CamKey()); //insert after current pos
	
	printf("Pointing to %i\n", flythroughKey);
	
	setKeyframe();
	dynamic_cast<QG::Slider*>(guiElements["frames"])->i = flythroughKey;
	dynamic_cast<QG::Slider*>(guiElements["frames"])->upper = f.keys.size() - 1;
	dynamic_cast<QG::Slider*>(guiElements["frames"])->setDirty();
}
void Scene::removeKeyframe()
{
	assert(currentFlythrough >= 0 && currentFlythrough < (int)flythroughs.size());
	Flythrough& f = flythroughs[currentFlythrough];
	if (f.keys.size())
	{
		f.keys.erase(f.keys.begin() + flythroughKey);
		flythroughKey = mymax(0, flythroughKey-1);
	}
}
void Scene::setCamera(Camera* cam)
{
	assert(cam);
	camera = cam;
}
void Scene::init()
{
	FileFinder::addDir(Config::getString("pyarlib-models"));
	VBOMeshOBJ::registerLoader();
	widget[0] = new VBOMesh();
	widget[1] = new VBOMesh();
	widget[2] = new VBOMesh();
	cameraIcon = new VBOMesh();
	widget[0]->load("widget_translate.obj");
	widget[1]->load("widget_rotate.obj");
	widget[2]->load("widget_scale.obj");
	cameraIcon->load("icon_camera.obj");
	widget[0]->upload();
	widget[1]->upload();
	widget[2]->upload();
	cameraIcon->upload();
	
	sphere->upload();
	
	std::set<std::string> modeltypes;
	modeltypes.insert("obj");
	modeltypes.insert("ctm");
	modeltypes.insert("3ds");
	
	std::vector<std::string> files = listDirectory(Config::getString("models"));
	for (size_t i = 0; i < files.size(); ++i)
	{
		if (files[i][0] != '.' && pathExists(files[i].c_str()))
		{
			printf("%s\n", files[i].c_str());
			std::vector<std::string> files2 = listDirectory(joinPath(Config::getString("models"), files[i]));
			for (size_t j = 0; j < files2.size(); ++j)
				files.push_back(joinPath(files[i], files2[j]));
		}
	}
	for (size_t i = 0; i < files.size(); ++i)
		if (contains(modeltypes, fileExtension(files[i])))
			availableModels.push_back(files[i]);
	for (size_t i = 0; i < availableModels.size(); ++i)
	{
		NewObjectDat* d = new NewObjectDat;
		d->owner = this;
		d->name = availableModels[i];
		QG::Button* b = new QG::Button(availableModels[i].c_str());
		b->capture(QG::CLICK, _addModel, d);
		guiElements["add"]->add(b);
	}
	
	hasInit = true;
}
Scene::Object Scene::parseObject(pugi::xml_node object)
{
	//create a mesh instance
	string meshName = object.attribute("mesh").value();
	vec3f pos(0.0f), rot(0.0f), scale(1.0f), resizeCover(0.0f);
	stringstream(object.attribute("position").value()) >> pos.x >> pos.y >> pos.z;
	stringstream(object.attribute("rotation").value()) >> rot.x >> rot.y >> rot.z;
	stringstream(object.attribute("scale").value()) >> scale.x >> scale.y >> scale.z;
	stringstream(object.attribute("resize-cover").value()) >> resizeCover.x >> resizeCover.y >> resizeCover.z;
	if (scale == vec3f(0.0f))
		scale = vec3f(1.0f);
		
	Object obj;
	obj.unlit = object.attribute("unlit").as_int() > 0;
	obj.backface = object.attribute("backface").as_int() > 0;
	obj.transform = mat44::translate(pos) * mat44::scale(scale) * mat44::rotate(rot);
	
	if (!getMesh(meshName, obj.mesh))
	{
		cout << "Warning: Object with unknown mesh, '" << meshName << endl;
	}
	else
	{
		for (size_t i = 0; i < obj.mesh->materials.size(); ++i)
			dynamic_cast<Material*>(obj.mesh->materials[i])->unlit = obj.unlit;
	}
	
	if (resizeCover != vec3f(0.0f))
	{
		//PRINTVEC3F(pos);
		//PRINTVEC3F(resizeCover);
		float coverScale = 99999999.0f;
		if (resizeCover.x > 0.0f) coverScale = mymin(resizeCover.x/obj.mesh->boundsSize.x, coverScale);
		if (resizeCover.y > 0.0f) coverScale = mymin(resizeCover.y/obj.mesh->boundsSize.y, coverScale);
		if (resizeCover.z > 0.0f) coverScale = mymin(resizeCover.z/obj.mesh->boundsSize.z, coverScale);
		mat44 coverTransform = mat44::scale(coverScale) * mat44::translate(-obj.mesh->center);
		obj.transform *= coverTransform;
	}
	return obj;
}
void Scene::updateObject(Object& o)
{
	o.globalTransform = mat44::scale(globalScale) * o.transform;
	o.globalTransformInv = o.globalTransform.inverse();
}
void Scene::saveFlythrough(int flythrough, std::string filename)
{
	if (flythrough < 0 || flythrough >= (int)flythroughs.size())
	{
		printf("Error: Cannot save flythrough %i: out of bounds\n", flythrough);
		return;
	}
	Flythrough& f = flythroughs[currentFlythrough];
	if (!filename.size())
	{
		if (f.filename.size())
			filename = f.filename;
		else
			filename = "flythrough-save.xml"; //default output
	}
    pugi::xml_document doc;
    pugi::xml_node node = doc.append_child();
    node.set_name("flythrough");
	for (int i = 0; i < (int)f.keys.size(); ++i)
	{
	    pugi::xml_node key = node.append_child();
	    key.set_name("key");
	    f.keys[i].toXML(&key);
	}
	doc.save_file(filename.c_str());
	printf("Saved \"%s\"\n", filename.c_str());
}
void Scene::loadFlythrough(pugi::xml_node* node, std::string srcfilename, std::string outfilename)
{
	printf("Loading Flythrough: %s\n", srcfilename.c_str());
	pugi::xml_node& flythrough = *node;
	
	Flythrough f;
	f.filename = outfilename;
	for (pugi::xml_node key = flythrough.child("key"); key; key = key.next_sibling("key"))
	{
		CamKey data;
		data.fromXML(&key);
		f.keys.push_back(data);
	}
	
	flythroughs.push_back(f);
	dynamic_cast<QG::Slider*>(guiElements["flythrough"])->upper = flythroughs.size() - 1;
	if (flythroughs.size() == 1)
		setFlythrough(0);
	computeFlythroughDistance(flythroughs.size()-1);
	
	/*
	string speedStr = flythrough.attribute("speed").value();
	if (speedStr.size() > 3 && speedStr.substr(speedStr.size()-3) == "fps")
		f.time = 1.0f/stringToFloat(speedStr.substr(speedStr.size()-3));
	else
		f.time = stringToFloat(speedStr);
	*/
}
void Scene::load(std::string filename)
{
	if (!hasInit)
		init();

	if (filename == "" || sceneName == filename)
		return;
	currentView = "none";
	sceneFilename = filename;
	sceneName = filename;

	VBOMeshOBJ::registerLoader();
	VBOMesh3DS::registerLoader();
	VBOMeshCTM::registerLoader();
	VBOMeshIFS::registerLoader();
	
	objects.clear();
	views.clear();
	lights.clear();
	flythroughs.clear();
	currentFlythrough = -1;
	
	VBOMesh* mesh;
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filename.c_str());
	if (!result || !doc.child("scene"))
	{
		cout << "Error: Could not open " << filename << endl;
		return;
	}
	pugi::xml_node scene = doc.child("scene");
	if (strlen(scene.attribute("name").value()))
		sceneName = scene.attribute("name").value();
		
	globalScale = scene.attribute("scale").as_float();
	if (globalScale <= 0.0f)
		globalScale = 1.0f;
	
	for (pugi::xml_node meshDec = scene.child("mesh"); meshDec; meshDec = meshDec.next_sibling("mesh"))
	{
		string name = meshDec.attribute("name").value();
		if (getMesh(name, mesh))
		{
			cout << "Note: Mesh '" << name << "' already defined in " << filename << endl;
			continue;
		}
		
		//load the mesh geometry
		pugi::xml_node file = meshDec.child("file");
		pugi::xml_node gen = meshDec.child("gen");
		if (file)
		{
			string src = file.attribute("src").value();
			if (!src.size())
			{
				cout << "Error: Inline mesh definition not implemeneted " << filename << endl;
				continue;
			}
			
			cout << "LOADING: " << src << endl;
			if (!mesh->load(src.c_str()))
			{
				cout << "Error: Could not load mesh '" << src << "' in " << filename << endl;
				continue;
			}
		}
		else if (gen)
		{
			printf("NOTE: Mesh generation in scene leaks\n");
			string type = gen.attribute("type").value();
			int dimx = gen.attribute("x").as_int();
			int dimy = gen.attribute("y").as_int();
			if (dimx == 0) dimx = 16;
			if (dimy == 0) dimy = 16;
			if (type == "plane")
				*mesh = VBOMesh::grid(vec2i(dimx, dimy), VBOMesh::paramPlane);
			else if (type == "sphere")
				*mesh = VBOMesh::grid(vec2i(dimx, dimy), VBOMesh::paramSphere);
			else if (type == "cube")
				*mesh = VBOMesh::cube();
			else
			{
				cout << "Error: Invalid gen type in mesh, in " << filename << endl;
				continue;
			}
			mesh->triangulate();
		}
		else
		{
			cout << "Warning: Empty model in " << filename << endl;
			continue;
		}
		
		//calc mesh stats and upload to GPU
		mesh->computeInfo();
		#if GENERATE_TANGENTS
		if (mesh->interleaved)
			mesh->uninterleave();
		mesh->generateTangents();
		#endif
		mesh->upload(false); //NOTE: leaves local data for ray tracing
		
		//load the material, if any
		pugi::xml_node material = meshDec.child("material");
		if (material)
		{		
			//FIXME: incomplete
			string diffuse = material.attribute("diffuse").value();
			string colour = material.attribute("colour").value();
			
			Material* mat;
			if (diffuse.size())
				mat = new Material(diffuse); //from texture
			else
				mat = new Material();
			
			if (colour.size())
				stringstream(colour) >> mat->colour.x >> mat->colour.y >> mat->colour.z >> mat->colour.w;
			//PRINTVEC4F(mat->colour);
			
			mat->upload(false); //NOTE: if not uploaded here, will auto-upload and free local
			mesh->setMaterial(mat);
		}
	}
	for (pugi::xml_node light = scene.child("light"); light; light = light.next_sibling("light"))
	{
		vec4f position;
		if (!(stringstream(light.attribute("position").value()) >> position.x >> position.y >> position.z >> position.w))
			cout << "Error: Light has no position in " << filename << endl;
		
		Light l;
		vec3f lookat, direction(-pi*0.5,0,0);
		if (stringstream(light.attribute("lookat").value()) >> lookat.x >> lookat.y >> lookat.z)
			l.zoomAt(position, lookat);
		else
		{
			stringstream(light.attribute("direction").value()) >> direction.x >> direction.y >> direction.z;
			l.setPosition(position);
			l.setRotation(direction);
		}
		
		float fov = light.attribute("fov").as_float();
		if (fov <= 0.0f || fov >= 180.0f)
			fov = 90.0f;
		
		l.setPerspective(fov*pi/180.0f);
		l.positional = position.w != 0.0f;
		l.scale(globalScale);
		l.regen();
		lights.push_back(l);
	}
	for (pugi::xml_node object = scene.child("object"); object; object = object.next_sibling("object"))
	{
		Object o = parseObject(object);
		updateObject(o);
		objects[nextObjectName++] = o;
	}
	for (pugi::xml_node object = scene.child("strip"); object; object = object.next_sibling("strip"))
	{
		//create a mesh instance
		Object obj = parseObject(object);
		
		/*
		string meshName = object.attribute("mesh").value();
		vec3f pos(0.0f), lpos(0.0f), rot(0.0f), scale(0.0f);
		int count = object.attribute("count").as_int();
		int deorder = object.attribute("deorder").as_int();
		stringstream(object.attribute("position").value()) >> pos.x >> pos.y >> pos.z;
		stringstream(object.attribute("last-position").value()) >> lpos.x >> lpos.y >> lpos.z;
		stringstream(object.attribute("rotation").value()) >> rot.x >> rot.y >> rot.z;
		stringstream(object.attribute("scale").value()) >> scale.x >> scale.y >> scale.z;
		if (scale == vec3f(0.0f))
			scale = vec3f(1.0f);
		*/
		
		vec3f pos(0.0f);
		vec3f lpos(0.0f);
		stringstream(object.attribute("position").value()) >> pos.x >> pos.y >> pos.z;
		stringstream(object.attribute("last-position").value()) >> lpos.x >> lpos.y >> lpos.z;
		int count = object.attribute("count").as_int();
		int deorder = object.attribute("deorder").as_int();
		
		//dupliate it
		for (int i = 0; i < count; ++i)
		{
			float f;
			
			//"shuffle" object order, affecting draw order
			if (deorder)
				f = (i%2==0?i/2:count-1-i/2) / (float)(count - 1);
			else
				f = i / (float)(count - 1);
			
			Object objInst = obj;
			
			vec3f p = (lpos - pos) * f;
			objInst.transform = mat44::translate(p) * objInst.transform;
			updateObject(objInst);
			objects[nextObjectName++] = objInst;
		}
	}
	
	//load all views
	for (pugi::xml_node view = scene.child("view"); view; view = view.next_sibling("view"))
	{
		string viewData = view.text().get();
	
		if (viewData.size())
		{
			views.push_back(View());
			views.back().scale = view.attribute("scale").as_int() > 0;
			views.back().name = view.attribute("name").value();
			views.back().data = viewData;
			views.back().focus = view.attribute("focus").as_float();
			views.back().fov = view.attribute("fov").as_float();
			
			float focalLength = 0.001f * CAMERA_DEFAULT_SENSOR_HEIGHT / (2.0f * tan(views.back().fov * 0.5f * pi / 180.0f));
			
			std::string aperture = pyarlib::trim(view.attribute("aperture").value());
			if (aperture[0] == 'f')
				views.back().aperture = focalLength / stringToFloat(aperture.substr(1));
			else
				views.back().aperture = stringToFloat(aperture);
		}
	}
	//load all flythroughs
	for (pugi::xml_node flythrough = scene.child("flythrough"); flythrough; flythrough = flythrough.next_sibling("flythrough"))
	{
		std::string src = flythrough.attribute("src").value();
		if (src.size())
		{
			std::string flythroughfile = joinPath(basefilepath(filename), src);
			pugi::xml_document fdoc;
			pugi::xml_parse_result fresult = fdoc.load_file(flythroughfile.c_str());
			if (!fresult || !fdoc.child("flythrough"))
				cout << "Error: Could not load flythrough from file " << flythroughfile << endl;
			else
			{
				pugi::xml_node flythroughnode = fdoc.child("flythrough");
				loadFlythrough(&flythroughnode, filename, flythroughfile);
			}
		}
		else
			loadFlythrough(&flythrough, filename, "");
	}

	int totalPolygons = 0;	
	for (Objects::iterator it = objects.begin(); it != objects.end(); ++it)
		totalPolygons += it->second.mesh->numPolygons;
	printf("TotalPolygons: %s = %i\n", sceneName.c_str(), totalPolygons);
}
void Scene::save(std::string filename)
{
}
bool Scene::raycastTool(vec4f from, vec4f to)
{
	float t1, t2;

	//first check tool intersection
	if (editObject < 0)
		return false;
	
	vec3f minPos;
	int minaxis = -1;
	float mindist = 999.0f;
	for (int i = 0; i < 3; ++i)
	{
		mat44 inv = toolmatInv[i] * objects[editObject].globalTransformInv;
		vec3f oFrom = inv * from;
		vec3f oDir = inv * to - oFrom;
		bool hit;
		if (editTool == 1)
		{
			float i1, i2;
			intersectCylinder(oFrom, oDir, vec3f(-0.2f,0.0f,0.0f), vec3f(1.0f,0.0f,0.0f), 1.0f-0.1f, 0.4f, i1, i2);
			hit = intersectCylinder(oFrom, oDir, vec3f(-0.1f,0.0f,0.0f), vec3f(1.0f,0.0f,0.0f), 1.0f, 0.2f, t1, t2) && ((t1 > 0.0f && t1 < i1) || (t2 > 0.0f && t2 > i2));
			if (!(t1 > 0.0f && t1 < i1))
				t1 = t2;
		}
		else
			hit = intersectCylinder(oFrom, oDir, vec3f(0.0f), vec3f(1.0f,0.0f,0.0f), 0.1f, 1.0f, t1, t2) && t1 > 0.0f;
		if (hit)
		{
			vec3f hit = oFrom+oDir*t1;
			debug = hit;
			if (t1 < mindist)
			{
				mindist = t1;
				minaxis = i;
				minPos = hit;
			}
		}
	}
	
	if (minaxis >= 0)
	{
		editPos = minPos;
		editAxis = minaxis;
		return true;
	}
	
	return false;
}
int Scene::raycastObjects(vec4f from, vec4f to)
{
	float t1, t2;
	float minDist = 999.0f;
	int minObject = -1;
	for (Objects::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		vec3f oFrom = it->second.globalTransformInv * from;
		vec3f oDir = it->second.globalTransformInv * to - oFrom;
		if (intersectSphere(oFrom, oDir, 0.4f, t1, t2) && t1 > 0.0f)
		{
			if (t1 < minDist)
			{
				minObject = it->first;
				minDist = t1;
			}
		}
	}
	return minObject;
}
void Scene::update(float dt)
{
	if (playing)
		moveFlythrough(dt);

	if (!editMode)
		return;
	
	if (!editActive)
	{
		if (jeltz->buttonDown("e"))
			editTool = 0;
		if (jeltz->buttonDown("r"))
			editTool = 1;
		if (jeltz->buttonDown("t"))
			editTool = 2;
	}
	
	if ((jeltz->mouseMoved() || jeltz->buttonDown("LButton")) && camera && !editActive)
	{
		mat44 toWorld = (camera->getProjection() * camera->getInverse()).inverse();
		vec2f mouse = (vec2f(jeltz->mousePos()) / vec2f(jeltz->winSize())) * 2.0f - 1.0f;
		mouse.y = -mouse.y;
		vec4f from = toWorld * vec4f(mouse, -1.0f, 1.0f);
		vec4f to = toWorld * vec4f(mouse, 1.0f, 1.0f);
		bool hit = raycastTool(from / from.w, to / to.w);
		if (jeltz->buttonDown("LButton") && hit)
		{
			editActive = true;
			jeltz->use("LButton");
		}
		if (jeltz->buttonDown("LButton") && !hit)
		{
			int found = raycastObjects(from / from.w, to / to.w);
			if (found >= 0)
				editObject = found;
		}
	}
	if (jeltz->mouseMoved() && editActive)
	{
		Objects::iterator it = objects.find(editObject);
		mat44 toEdit = (camera->getProjection() * camera->getInverse() * it->second.globalTransform * toolmat[editAxis]).inverse();
		vec2f mouse = (vec2f(jeltz->mousePos()) / vec2f(jeltz->winSize())) * 2.0f - 1.0f;
		mouse.y = -mouse.y;
		vec4f from = toEdit * vec4f(mouse, -1.0f, 1.0f);
		vec4f to = toEdit * vec4f(mouse, 1.0f, 1.0f);
		vec3f pos = from.xyz()/from.w;
		vec3f dir = to.xyz()/to.w - pos;
		float t1, t2;
		if (editTool == 1)
		{
			vec3f plane = pos - dir * (pos.x - editPos.x) / dir.x;
			vec2f v1 = editPos.yz().unit();
			vec2f v2 = plane.yz().unit();
			float sinAngle = (vec3f(v1,0.0f).cross(vec3f(v2, 0.0f))).z;
			float cosAngle = v1.dot(v2);
			float angle = atan2(sinAngle, cosAngle);
			editTransform = toolmat[editAxis] * mat44::rotate(angle, vec3f(1.0f, 0.0f, 0.0f)) * toolmatInv[editAxis];
		}
		else
		{
			closestPointsLines(pos, dir, vec3f(0.0f), vec3f(1.0f, 0.0f, 0.0f), t1, t2);
			vec3f nearest = pos + dir * t1;
			printf("%f %f\n", nearest.x, editPos.x);
			if (editTool == 0)
				editTransform = toolmat[editAxis] * mat44::translate(vec3f(nearest.x - editPos.x, 0.0f, 0.0f)) * toolmatInv[editAxis];
			else
				editTransform = toolmat[editAxis] * mat44::scale(vec3f(mymax(nearest.x / mymax(editPos.x, 0.01f), 0.01f))) * toolmatInv[editAxis];
				//editTransform = toolmat[editAxis] * mat44::scale(vec3f(mymax(nearest.x / mymax(editPos.x, 0.01f), 0.01f), 1.0f, 1.0f)) * toolmatInv[editAxis];
		}
		jeltz->use("LButton");
	}
	if (jeltz->buttonDown("RButton") && editActive)
	{
		editActive = false;
		jeltz->use("RButton");
		editTransform = mat44::identity();
	}
	if (jeltz->buttonUp("LButton") && editActive)
	{
		editActive = false;
		jeltz->use("LButton");
		Objects::iterator it = objects.find(editObject);
		it->second.globalTransform *= editTransform;
		it->second.globalTransformInv = it->second.globalTransform.inverse();
		editTransform = mat44::identity();
	}
}
void Scene::display()
{
	/*
	if (currentFlythrough >= 0 && currentFlythrough < (int)flythroughs.size())
	{
		glEnable(GL_DEPTH_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);
		Flythrough& f = flythroughs[currentFlythrough];
		widgetShader.use();
		widgetShader.set("lightPos", vec4f(0,0,0,1));
		widgetShader.set("projectionMat", camera->getProjection());
		widgetShader.set("colourIn", vec4f(1,1,1,1));
		for (int i = 0; i < (int)f.keys.size(); ++i)
		{
			widgetShader.set("modelviewMat", camera->getInverse() * f.keys[i].transform);
			widgetShader.set("normalMat", mat33(camera->getTransform().transpose()));
			cameraIcon->draw();
		}
		widgetShader.unuse();
		
		
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(camera->getProjection().m);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(camera->getInverse().m);
		for (int i = 1; i < (int)f.keys.size(); ++i)
		{
			vec3f a = f.keys[i-1].position;
			vec3f b = f.keys[i].position;
			vec3f aDir = i > 1 ? b - f.keys[i-2].position : b - a;
			vec3f bDir = i < (int)f.keys.size()-1 ? f.keys[i+1].position - a : b - a;
			glBegin(GL_LINE_STRIP);
			for (int j = 0; j < splineIntegrateSteps; ++j)
			{
				vec3f v = interpHermite(a, aDir * 0.5, b, bDir * 0.5, j / (float)(splineIntegrateSteps-1));
				glVertex3fv((float*)&v);
			}
			glEnd();
		}
	}
	*/

	if (!editMode)
		return;
	
	Objects::iterator it = objects.find(editObject);
	if (it == objects.end())
		return;
	
	Object& edit = it->second;
	
	mat44 mv, mvi;
	if (camera)
	{
		mv = camera->getInverse() * edit.globalTransform * editTransform;
		mvi = editTransform.inverse() * edit.globalTransformInv * camera->getTransform();
	}
	
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	
	widgetShader.use();
	widgetShader.set("lightPos", vec4f(0,0,0,1));
	widgetShader.set("projectionMat", camera->getProjection());
	widgetShader.set("colourIn", editAxis == 0 ? vec4f(1,0,0,1) : vec4f(0.5,0,0,1));
	widgetShader.set("modelviewMat", mv);
	widgetShader.set("normalMat", mat33(mvi.transpose()));
	widget[editTool]->draw();
	widgetShader.set("colourIn", editAxis == 1 ? vec4f(0,1,0,1) : vec4f(0,0.5,0,1));
	widgetShader.set("modelviewMat", mv * toolmat[1]);
	widgetShader.set("normalMat", mat33((toolmatInv[1] * edit.globalTransformInv * mvi).transpose()));
	widget[editTool]->draw();
	widgetShader.set("colourIn", editAxis == 2 ? vec4f(0,0,1,1) : vec4f(0,0,0.5,1));
	if (editTool == 11234)
	{
		widgetShader.set("modelviewMat", mv * toolmat[3]);
		widgetShader.set("normalMat", mat33((toolmatInv[3] * mvi).transpose()));
	}
	else
	{
		widgetShader.set("modelviewMat", mv * toolmat[2]);
		widgetShader.set("normalMat", mat33((toolmatInv[2] * mvi).transpose()));
	}
	widget[editTool]->draw();
	
	widgetShader.set("modelviewMat", mv * mat44::translate(debug));
	widgetShader.set("normalMat", mat33((mv * mat44::translate(debug)).inverse().transpose()));
	sphere->draw();
	
	widgetShader.unuse();
}
bool Scene::getMesh(std::string name, VBOMesh*& mesh)
{
	if (meshes.find(name) != meshes.end())
	{
		mesh = meshes[name];
		return true;
	}
	meshes[name] = mesh = new VBOMesh();
	return false;
}
void Scene::draw(Shader* shader, Camera* cam)
{
	mat44 mv, mvi;
	if (!cam)
		cam = camera;
	if (cam)
	{
		mv = cam->getInverse();
		mvi = cam->getTransform();
		shader->set("projectionMat", cam->getProjection());
	}
	else
	{
		mv = mat44::identity();
		mvi = mat44::identity();
	}
	if (useLights)
	{
		vec4f esLight(0,0,0,1);
		if (lights.size())
		{
			esLight = mv * vec4f(lights[0].getZoomPos(), lights[0].positional ? 1.0f : 0.0f);
			mat44 lightMat = lights[0].getProjection() * lights[0].getInverse() * mvi;
			shader->set("lightMat", lightMat);
		}
		shader->set("lightPos", esLight);
	}
	for (Objects::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		shader->set("unlit", it->second.unlit);
		if (forceDoubleSided || it->second.backface)
			glDisable(GL_CULL_FACE);
		else
			glEnable(GL_CULL_FACE);
		if (editMode && it->first == editObject)
		{
			shader->set("modelviewMat", mv * it->second.globalTransform * editTransform);
			shader->set("normalMat", mat33((editTransform.inverse() * it->second.globalTransformInv * mvi).transpose()));
		}
		else
		{
			shader->set("modelviewMat", mv * it->second.globalTransform);
			shader->set("normalMat", mat33((it->second.globalTransformInv * mvi).transpose()));
		}
		it->second.mesh->draw();
	}
}
void Scene::draw()
{
	if (!camera)
	{
		printf("Error: Scene missing camera. Call setCamera()\n");
		return;
	}
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(camera->getProjection().m);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(camera->getInverse().m);
	for (Objects::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		glPushMatrix();
		mat44 t;
		if (editMode && it->first == editObject)
			t = it->second.globalTransform * editTransform;
		else
			t = it->second.globalTransform;
		glMultMatrixf(t.m);
		it->second.mesh->draw();
		glPopMatrix();
	}
}
void Scene::drawShadow(Shader* shader)
{
	if (!lights.size())
		return;
	draw(shader, &lights[0]);
}
void Scene::trace(std::string filename)
{
	if (!camera)
	{
		printf("Error: Cannot trace() scene without camera\n");
		return;
	}
	
	if (!filename.size())
		filename = "render_" + basefilename(sceneName) + "_" + currentView + ".png";
	
	vec4i vp;
	glGetIntegerv(GL_VIEWPORT, (GLint*)&vp);

	TraceScene* tracer = new TraceScene();
	for (Objects::iterator it = objects.begin(); it != objects.end(); ++it)
		tracer->addMesh(it->second.mesh, it->second.globalTransform);
	
	for (size_t l = 0; l < lights.size(); ++l)
		tracer->addLight(mat44::translate(lights[0].getZoomPos()), vec3f(1), 1, 0.0f, true);
	
	tracer->build();

	QI::ImagePNG out;
	out.resize(vp.z, vp.w);
	//out.resize(vp.z>>1, vp.w>>1);
	camera->setAspectRatio(vp.z / (float)vp.w);
	camera->regen();
	tracer->dof.aperture = camera->aperture;
	tracer->dof.focus = camera->focus;
	tracer->dof.samples = pow(vp.w * camera->getNearSize().y / tracer->dof.aperture, 2.0f);
	printf("Samples: %i\n", tracer->dof.samples);
	tracer->dof.samples = mymin(tracer->dof.samples, 256);
	
	tracer->shadowScale = 0.5f;

	tracer->render(&out, camera, 4);
	printf("Rendering...\n");
	while (true)
	{
		float p = tracer->getProgress();
		printf("%.2f%%\n", p*100.0f);
		mysleep(1.0f);
		if (p == 1.0f)
			break;
	}
	printf("Done\n");
	out.saveImage(filename);
	
	delete tracer;
}
void Scene::add(std::string modelname)
{
	if (!hasInit)
		init();
		
	printf("Adding %s to scene\n", modelname.c_str());
	Object obj;
	obj.globalTransform = mat44::identity();
	obj.globalTransformInv = mat44::identity();
	obj.transform = mat44::identity();
	if (!getMesh(modelname, obj.mesh))
	{
		obj.mesh->load(modelname.c_str());
		obj.mesh->computeInfo();
		#if GENERATE_TANGENTS
		if (obj.mesh->interleaved)
			obj.mesh->uninterleave();
		obj.mesh->generateTangents();
		#endif
		obj.mesh->transform(mat44::scale(vec3f(1.0f/obj.mesh->boundsSize.y)) * mat44::translate(-obj.mesh->center));
		obj.mesh->upload(false); //NOTE: leaves local data for ray tracing
	}
	objects[nextObjectName++] = obj;
}
void Scene::edit(bool enable)
{
	editMode = enable;
	controls->hide(!enable);
}
int Scene::getNumViews()
{
	return (int)views.size();
}
std::string Scene::getViewName(int i)
{
	//assert(i >= 0 && i < (int)views.size());
	if (i < 0 || i >= (int)views.size())
		return "out-of-bounds";
	return views[i].name;
}
std::string Scene::getName()
{
	return sceneName;
}
std::string Scene::getFilename()
{
	return sceneFilename;
}
void Scene::setView(int i)
{
	assert(i >= 0 && i < (int)views.size());
	
	currentView = views[i].name;
	
	float camScale = views[i].scale ? globalScale : 1.0f;
	stringstream(views[i].data) >> *camera;
	
	if (camera->isPerspective())
	{
		camera->setDistance(0.01f, 100.0f); //not sure I want to overwrite this
	
		float fov = views[i].fov*pi/180.0f;
		if (fov > 0.0f && fov < pi)
			camera->setFOV(fov);
		else
			camera->setFOV(CAMERA_DEFAULT_FOV);
	}
	
	camera->scale(camScale);
	camera->focus = views[i].focus * camScale;
		
	camera->aperture = views[i].aperture;
	
	camera->regen();
}
void Scene::enableLighting(bool enable)
{
	useLights = enable;
}
Camera* Scene::getCamera()
{
	return camera;
}

}
