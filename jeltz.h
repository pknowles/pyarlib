/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

//no, I couldn't think of a less descriptive name
//I built this so I don't have to rewrite window
//management, input handling etc for every project I make.
//the idea is to make the code in main.cpp (or some graphics demo) as short as possible at all costs!
//as such, there are some seemingly hard coded areas

#ifndef JELTZ_H
#define JELTZ_H

#include <string>
#include <set>
#include <list>

// default SDL window arguments
#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600
#define DEFAULT_DEPTH 24
#define DEFAULT_STENCIL 8

#define GL3_PROTOTYPES 1
#define GL4_PROTOTYPES 1 //maybe??

//feel free to change this - I put my 1.3 includes and libs in separate directories (include/SDL13/...)
//this is mainly because most apps require 1.2 but I've been leaning towards 1.3
//lately because it's got some nice features and I have less bugs porting to windows
#include <SDL2/SDL.h>

//if SDL 1.3 (renamed to SDL 2 now)
#if (SDL_MAJOR_VERSION == 2)
#define JELTZ_USE_SDL13 1
#else
#define JELTZ_USE_SDL13 0
#endif

//no idea why SDL wants a main() hook
#undef main

#include "vec.h"

class Jeltz;

//allows external tools to be created. supplies update() to plugins before the main update() callback
class JeltzPlugin
{
	friend class Jeltz;
protected:
	Jeltz* jeltz;
	virtual void init();
	virtual void update(float dt);
	virtual void display();
	virtual void cleanup();
	JeltzPlugin();
	virtual ~JeltzPlugin();
public:
	bool enable;
};

class Jeltz
{
public:
	typedef void (*CallbackVoid)(void);
	typedef void (*CallbackFloat)(float);
	void add(JeltzPlugin* plugin);
private:
	std::set<std::string> keysDown;
	std::set<std::string> keyStates;
	std::set<std::string> keysUp;

	struct Display {
		vec2i position;
		vec2i size;
	};
	std::vector<Display> displays;

	float sleepTime;
	float minFrameTime; //vsync alternative. call limit()
	bool isMouseDown; //any mouse button
	vec2i desktopRes;
	vec2i pendingResize;
	vec2i pendingMove;
	vec2i windowedSize;
	vec2i windowSize;
	vec2i mousePosition;
	vec2i mouseDelta;
	vec2i mouseWheelDelta;
	JeltzPlugin* plugins[32];
	int numPlugins;
	bool changeFullscreen;
	bool isFullscreen;
	bool isBorderless;
	bool hasFocus;
	bool renderUnfocused;
	bool hasInit;
	bool hasResized;
	bool hasResizedNF; //resize while not in focus
	bool justWarped;
	bool debugEvents;
	bool forceNextDraw;
	char* windowTitle;
	void focusChanged(bool inFocus);
#if JELTZ_USE_SDL13
	SDL_Window* window;
	SDL_GLContext glcontext;
	void processEventWindow(SDL_WindowEvent& window);
	void processEventMouseWheel(SDL_MouseWheelEvent& wheel);
#else
	bool ignoreNextSDLWindowResizeEvenet;
	int surfaceFlags;
	int surfaceBpp;
	SDL_Surface* surface;
	void processEventActive(SDL_ActiveEvent active);
	void processEventResize(SDL_ResizeEvent resize);
	void processEventExpose(SDL_ExposeEvent expose);
#endif
	bool running;
	CallbackVoid callbackCleanup;
	CallbackVoid callbackDisplay;
	CallbackFloat callbackUpdate;
	const char* getButtonName(Uint8 button);
	void inputDown(const char* name);
	void inputUp(const char* name);
	void updateWindow(); //handles fullscreen change, movement etc
	void processEventKeyboard(SDL_KeyboardEvent& keyboard);
	void processEventMouseMove(SDL_MouseMotionEvent& motion);
	void processEventMouseButton(SDL_MouseButtonEvent& btn);
	void processEvents();
	void reshape(int w, int h);
	void doMove(int x, int y);
	void doResize(int w, int h);
public:

	//constructor with window title
	Jeltz(const char* name);
	virtual ~Jeltz();
	
	//call these from main() in order
	bool init();
	void run();
	
	//set the callback functions. similar to glut, ugly, but quick to implement and instantiate
	void setCleanup(CallbackVoid callback) {callbackCleanup = callback;}
	void setDisplay(CallbackVoid callback) {callbackDisplay = callback;}
	void setUpdate(CallbackFloat callback) {callbackUpdate = callback;}
	
	//commonly called by a plugin to "use" (really "steal") an event (removes it from the pipeline)
	void use(const char* name);
	
	//check if button is down (by name, eg "Escape" or "W")
	//uses SDL_GetKeyName (mostly) for names. call printEvents() to print names
	//see: http://wiki.libsdl.org/moin.cgi/SDL_Scancode
	bool button(const char* name);
	bool buttonDown(const char* name);
	bool buttonUp(const char* name);
	vec2i mousePos(); //mouse position
	vec2f mousePosN(); //normalized mouse position, with origin bottom left
	vec2i mouseMove(); //mouse delta
	vec2i mouseWheel(); //wheel delta
	bool mouseMoved();
	
	bool resized(); //has window been resized between now and the last frame?
	bool focused(); //does the window have focus? (eg not being resized or alt tabbed)
	
	vec2i winSize(); //returns current window size
	
	//quit after this frame
	void quit();
	
	void limit(float frameTime); //set minimum frame time (in seconds). zero to disable
	void vsync(bool enable = true); //call after init()
	void resize(vec2i size);
	void resize(int width, int height);
	void move(vec2i pos);
	void move(int x, int h);
	bool getFullscreen() {return isFullscreen;}
	void fullScreen(bool enable = true); //call after init()
	void maximize();
	bool getBorderless() {return isBorderless;}
	void removeBorder(bool enable = true);
	void printEvents(bool enable = true); //use for debugging and to get event names
	void postUnfocusedRedisplay(); //use this to force drawing when window has no focus
	void redrawUnfocused(bool enable = true);
};

#endif
