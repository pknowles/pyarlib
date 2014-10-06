/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <set>

#include "jeltzgui.h"
#include "texture.h"
#include "profile.h"
#include "util.h"
#include "config.h"
#include "findfile.h"

#include <GL/gl.h>


#define PROFILE_GPU_TIME 0

#if PROFILE_GPU_TIME
static Profile profile;
#endif

JeltzGUI::JeltzGUI() : speedup(Config::getString("font"), 24)
{
	visible = true;
	body.fill = QG::NONE;
	body.add(controls);
	body.stack = QG::HORIZONTAL;
	controls.add(fps);
	controls.add(fpsNum);
	controls.fill = QG::BOTH;
	fpsNum.fill = QG::NONE;
	fpsNum.border = 8;
	fpsNum.width = fps.width - fpsNum.padding.left - fpsNum.padding.right;
	ignoring = true;
	speedupTimer = 0.0f;
	drawSpeedup = true;
}
JeltzGUI::~JeltzGUI()
{
}
void JeltzGUI::ignoreNextTime()
{
	ignoring = true;
}
void JeltzGUI::init()
{
/*
	QG::widgetBox.setImage(new QG::NineBoxImage("pyarlib/img/button.png", 38, 4));
	QG::labelBox.setImage(new QG::NineBoxImage("pyarlib/img/label.png", 38, 1));
	QG::buttonBox.setImage(new QG::NineBoxImage("pyarlib/img/button.png", 38, 4));
	QG::checkBox.setImage(new QG::NineBoxImage("pyarlib/img/check.png", 0, 2));
	QG::radioBox.setImage(new QG::NineBoxImage("pyarlib/img/radio.png", 0, 2));
	
	QG::fpsgraphBox.setImage(new QG::NineBoxImage("pyarlib/img/fps.png", 8, 1));
*/
	std::string guifile = Config::getString("gui");
	std::string result = FileFinder::find(guifile);
	if (result.size())
		controls.loadImages(result);
	else
		printf("Error: missing %s\n", guifile.c_str());
}
void JeltzGUI::postSpeedup(float x)
{
	if (speedupTimer > 0.0f && ((x < 1.0f && x > currentSpeedup) || (x > 1.0f && x < currentSpeedup)))
		return;
	currentSpeedup = x;
	
	if (x > 1.0f)
		speedup.colour = vec4f(157/255.0f, 224/255.0f, 137/255.0f, 1.0f);
	else
		speedup.colour = vec4f(223/255.0f, 148/255.0f, 161/255.0f, 1.0f);
	speedup.colour *= vec4f(0.5, 0.5, 0.5, 1.0f);
	speedupTimer = 2.0f;
	speedup.textf("x%.2f (%i%%)", x, (int)(100.0f/x));
	//printf("Speedup %f\n", x);
}
void JeltzGUI::update(float dt)
{
	if (jeltz->resized())
		ignoreNextTime();

	if (!ignoring && jeltz->focused())
	{
		fps.update(dt);
	}
	
	
		
	if (fps.gotNewSample)
	{
		int n = GUIFPS_NUM_SAMPLES / 2;
		int o = GUIFPS_NUM_SAMPLES + fps.currentSample - n - 2; //sub 2 to ignore transition times
		int o2 = GUIFPS_NUM_SAMPLES + fps.currentSample - n / 2;
		
		float suma=0.0f, suma2=0.0f, sumb=0.0f, sumb2=0.0f;
		for (int i = 0; i < n/2; ++i)
		{
			suma += fps.times[(o+i)%GUIFPS_NUM_SAMPLES];
			suma2 += fps.times[(o+i)%GUIFPS_NUM_SAMPLES] * fps.times[(o+i)%GUIFPS_NUM_SAMPLES];
			sumb += fps.times[(o2+i)%GUIFPS_NUM_SAMPLES];
			sumb2 += fps.times[(o2+i)%GUIFPS_NUM_SAMPLES] * fps.times[(o2+i)%GUIFPS_NUM_SAMPLES];
			//printf("a %.2f\n", speedupFrames[i]*1000.0f);
			//printf("b %.2f\n", speedupFrames[n/2+i]*1000.0f);
		}
		float meana = suma / (n/2);
		float stdeva = mymax(meana * 0.01f, (float)sqrt(myabs(suma2/(n/2) - meana*meana)));
		float meanb = sumb / (n/2);
		float stdevb = mymax(meanb * 0.01f, (float)sqrt(myabs(sumb2/(n/2) - meanb*meanb)));
		
		float diff = myabs(meanb - meana);
		
		//printf("%.2f %.2f %.2f %.2f %.2f\n", diff*1000.0f, meana*1000.0f, stdeva*1000.0f, meanb*1000.0f, stdevb*1000.0f);
		
		if (diff > stdeva && diff > stdevb)
		{
			float speedup = meana / meanb;
			postSpeedup(speedup);
		}
	}
	
	if (speedupTimer > 0.0f)
		speedupTimer -= dt;

	#if PROFILE_GPU_TIME
	profile.time("GPUStuff");
	if (!ignoring)
		profile.begin();
	#endif
	
	ignoring = false;
	
	if (fps.gotNewSample)
	{
		float allTime = fps.tpfLast * 1000.0f;
		
		#if PROFILE_GPU_TIME
		float gpuTime = profile.get("GPUStuff");
		float gpuRatio = myclamp(gpuTime / allTime, 0.0f, 1.0f);
		fpsNum.textf("fps: %.1f\nms: %.2f\ngpu: %.1f%%", fps.fpsLast, allTime, gpuRatio * 100.0f);
		#else
		fpsNum.textf("fps: %.1f\nms: %.2f", fps.fpsLast, allTime);
		#endif
	}
	
	if (jeltz->resized())
	{
		body.width = jeltz->winSize().x;
		body.height = jeltz->winSize().y;
		body.setDirty();
		modelview =
			mat44::translate(-1.0, 1.0, 0.0) *
			mat44::scale(1, -1, 1) *
			mat44::scale(2.0f/jeltz->winSize().x, 2.0f/jeltz->winSize().y, -0.01);
	}
	
	if (visible)
	{
		bool handled = false;
		if (jeltz->buttonDown("LButton"))
			if (QG::Widget::allMouseDown(jeltz->mousePos()))
				handled = true;
			
		if (jeltz->mouseMove() != vec2i(0))
			if (QG::Widget::allMouseMove(jeltz->mousePos()))
				handled = true;
			
		if (jeltz->buttonUp("LButton"))
			QG::Widget::allMouseUp(jeltz->mousePos());
		
		//FIXME: don't really like this - there's no control over which LButton event gets "used"
		if (handled)
			jeltz->use("LButton");
	}
}

void JeltzGUI::display()
{
	CHECKERROR;
	
	glClear(GL_DEPTH_BUFFER_BIT);
	
	glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_COLOR_BUFFER_BIT);

	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	if (visible)
	{
		QG::Widget::draw(modelview);
	
		if (drawSpeedup && speedupTimer > 0.0f)
		{
		
			speedup.colour.w = mymin(speedupTimer * 2.0f, 1.0f);
			vec2i pos(jeltz->winSize().x/2 + speedup.boundsMin.x - (speedup.boundsMax.x-speedup.boundsMin.x) / 2, jeltz->winSize().y / 2 + speedupTimer * 30);
			speedup.draw(modelview * mat44::translate(pos.x, pos.y, 0) * mat44::scale(1, -1, 1));
		}
	}
	
	glPopAttrib();
	glPopAttrib();
	
	CHECKERROR;
}

void JeltzGUI::cleanup()
{
	QG::Widget::releaseAll();
}

