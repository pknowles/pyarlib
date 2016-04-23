
#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <map>
#include <string>

#include "jeltz.h"
#include "matrix.h"
#include "quaternion.h"
#include "vec.h"
#include "camera.h"

//uugh http://stackoverflow.com/questions/8948493/visual-studio-doesnt-allow-me-to-use-certain-variable-names
#undef far
#undef near

struct VBOMesh;
class Shader;

namespace pugi { class xml_node; }

namespace QG {class Widget;}

namespace pyarlib
{

class Scene : public JeltzPlugin
{
private:
	struct NewObjectDat {
		Scene* owner;
		std::string name;
	};
	struct Object
	{
		mat44 globalTransform;
		mat44 globalTransformInv;
		mat44 transform;
		VBOMesh* mesh;
		int parent;
		bool unlit;
		bool backface;
	};
	struct Light : Camera
	{
		enum Type {
			DIRECTIONAL,
			SPOT,
			POINT,
			AREA
		} type;
		vec3f colour;
		bool positional;
	};
	
	typedef std::map<int, Object> Objects;
	typedef std::map<std::string, VBOMesh*> Meshes;
	
	Meshes meshes;
	std::vector<Light> lights;
	bool useLights;
	bool hasInit;
	bool editMode;
	int editTool;
	int editObject;
	int editAxis;
	bool editActive;
	vec3f editPos;
	float globalScale;
	mat44 editTransform;
	
	mat44 toolmat[3];
	mat44 toolmatInv[3];
	
	int nextObjectName;
	
	QG::Widget* controls;
	std::map<std::string, QG::Widget*> guiElements;
	Camera* camera;
	VBOMesh* widget[3]; //3D controls
	VBOMesh* cameraIcon;
	
	std::vector<std::string> availableModels;
	
	struct View {
		bool scale;
		std::string name;
		std::string data;
		float focus;
		float aperture;
		float fov;
		float near, far;
		float orth_size;
	};
	std::vector<View> views;
	
	struct CamKey {
		//serialized values
		vec3f position;
		Quat rotation;
		mat44 transform;
		float aperture;
		float focus;
		float speed;
		int interp;
		void toXML(pugi::xml_node* node);
		void fromXML(pugi::xml_node* node);
		
		//these are generated
		float startTime;
		float distance;
		float time;
	};
	struct Flythrough {
		std::vector<CamKey> keys;
		std::string filename;
		float distance;
		float time;
	};
	std::vector<Flythrough> flythroughs;
	bool playing;
	int splineIntegrateSteps;
	int currentFlythrough;
	int flythroughKey;
	float flythroughTime;
	
	std::string currentView;
	std::string sceneName;
	std::string sceneFilename;
	
	static void _changeFlythrough(QG::Widget* w, void* dat);
	static void _addModel(void* dat);
	static void _changeKey(QG::Widget* w, void* dat);
	static void _changeRatio(QG::Widget* w, void* dat);
	static void _addKey(void* dat);
	static void _repKey(void* dat);
	static void _delKey(void* dat);
	static void _saveFlythrough(void* dat);
	void computeFlythroughDistance(int flythrough, int keyframe = -1); //if keyframe is not -1, compute only for adjacent keys
	Scene(const Scene& other) {};
	void operator=(const Scene& other) {};
	bool raycastTool(vec4f from, vec4f to);
	int raycastObjects(vec4f from, vec4f to);
	Object parseObject(pugi::xml_node object);
	void updateObject(Object& o);
	void loadFlythrough(pugi::xml_node* node, std::string srcfilename, std::string outfilename);
protected:
	virtual void init();
	virtual void update(float dt);
	virtual void display();
	bool getMesh(std::string name, VBOMesh*& mesh);
public:
	Objects objects;
	bool forceDoubleSided;
	Scene();
	virtual ~Scene();
	int getNumViews();
	int getNumFlythroughs();
	void setFlythrough(int i);
	void playFlythrough(int i);
	void stopFlythrough();
	void jumpFlythrough(int keyframe, float ratio = 0.0f);
	void jumpFlythrough(float ratio); //jump to 0 to 1 position within whole flythrough
	bool moveFlythrough(float time);
	void setKeyframe();
	void insertKeyframe();
	void removeKeyframe();
	void saveFlythrough(int flythrough, std::string filename = ""); //set filename to override default or previous
	bool isPlayingFlythrough() {return playing;}
	std::string getViewName(int i);
	std::string getName();
	std::string getFilename();
	void enableLighting(bool enable = true);
	void setView(int i);
	void setCamera(Camera* cam);
	bool load(std::string filename);
	void save(std::string filename);
	void draw(Shader* shader, Camera* cam = NULL);
	void draw();
	void drawShadow(Shader* shader);
	void edit(bool enable);
	void add(std::string modelname);
	void trace(std::string filename = "");
	Camera* getCamera();
	operator QG::Widget&() {return *controls;}
};

}

#endif
