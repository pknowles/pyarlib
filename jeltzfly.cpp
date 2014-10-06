/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include "jeltzfly.h"

#include <fstream>
#include <ostream>

#include "includegl.h"
#include "shader.h"
#include "util.h"

using namespace std;

const float to_rad = pi/180.0f;

JeltzFly::JeltzFly()
{
	camera.setPerspective(60.0f * (pi/180.0f));
	camera.setDistance(0.01f, 100.0f);
	camera.regen();
	
	sensitivity = 0.5;
	speed = 1.0;
	interpolating = false;
	snapSpeed = 5.0;
	orbit = vec3f(0.0f);
	saveOnExit = true;
	loadCamera();
}
JeltzFly::~JeltzFly()
{
	if (saveOnExit)
		saveCamera();
}
void JeltzFly::update(float dt)
{
	bool moved = false;
	float mspeed = speed;
	
	//increase wasd speed with shift
	if (jeltz->button("Left Ctrl"))
		mspeed *= 0.1;
	if (jeltz->button("Left Shift"))
		mspeed *= 10.0;
		
	//standard wasd fly movement
	if (jeltz->button("A"))
		{camera.move(camera.rightVec() * mspeed * -dt); moved = true;}
	if (jeltz->button("D"))
		{camera.move(camera.rightVec() * mspeed * dt); moved = true;}
	if (jeltz->button("S"))
		{camera.move(camera.toVec() * mspeed * -dt); moved = true;}
	if (jeltz->button("W"))
		{camera.move(camera.toVec() * mspeed * dt); moved = true;}
	
	//un-orbit with wasd, toggle wth space
	bool toggleOrbit = jeltz->buttonDown("Space");
	bool orbiting = camera.getZoom() > 0.0;
	if ((toggleOrbit || moved) && orbiting)
	{
		camera.setPosition(camera.getZoomPos());
		camera.setZoom(0.0);
		moved = true;
	}
	else if (toggleOrbit && !orbiting)
	{
		interpolating = true;
		//interpFrom = Quat(camera.getTransform()).inverse();
		interpFrom = Quat::dirUp(camera.toVec(), camera.upVec());
		//interpFrom = Quat::fromEuler(camera.getRotation());
		vec3f rotateTowards = (orbit - camera.getPosition()).unit();
		
		if (myabs(rotateTowards.y) > 0.9999f)
		{
			if (myabs(camera.upVec().y) > 0.9999f)
			{
				printf("Using final.up = initial.forward\n");
				interpTo = Quat::dirUp(rotateTowards, camera.toVec() * mysign(rotateTowards.y));
			}
			else
			{
				printf("Using final.up = initial.up\n");
				interpTo = Quat::dirUp(rotateTowards, camera.upVec() * mysign(camera.upVec().y));
			}
		}
		else
		{
			//printf("Using final.up = (0,1,0)\n");
			interpTo = Quat::dirUp(rotateTowards);
		}
		camera.setOffset(Quat::identity());
		//interpTo = camera.getOffset().inverse() * interpTo;
		interpFrom.normalize();
		interpTo.normalize();
		interpRatio = 0.0f;
		float a = (interpTo * interpFrom.inverse()).unit().getAngle();
		printf("Interp Angle = %.2fdeg\n", a * 180.0f/pi);
		a = std::max(0.01f, a);
		msnapSpeed = snapSpeed / a;
		camera.lock(false);
	}
	
	//middle mouse pan
	if (jeltz->button("MButton"))
	{
		vec2i move = jeltz->mouseMove();
		if (move.x != 0 || move.y != 0)
		{
			float msensitivity = 1.0f;
			if (jeltz->button("Left Ctrl"))
				msensitivity *= 0.1f;
			if (jeltz->button("Left Shift"))
				msensitivity *= 10.0f;
			vec2f dir;
			if (camera.getZoom() > 0.1f)
				dir = vec2f(tan(camera.getFOVX()*0.5f), tan(camera.getFOVY()*0.5f)) * (2.0f * camera.getZoom() * msensitivity) * vec2f(-move.x, move.y) / jeltz->winSize();
			else
				dir = vec2f(-move.x, move.y) * msensitivity * mspeed * sensitivity * 0.01f;
			camera.move(camera.upVec() * dir.y + camera.rightVec() * dir.x);
			moved = true;
		}
	}
	
	//left mouse rotate
	if (jeltz->button("LButton"))
	{
		vec2i move = jeltz->mouseMove();
		if (move.x != 0 || move.y != 0)
		{
			float msensitivity = sensitivity;
			if (jeltz->button("Left Ctrl"))
				msensitivity *= 0.1;
			camera.rotate(vec2f(-move.y, -move.x) * to_rad * msensitivity);
			moved = true;
		}
	}
	
	//right mouse zoom
	if (jeltz->button("RButton"))
	{
		vec2i move = jeltz->mouseMove();
		if (move.y != 0)
		{
			float msensitivity = sensitivity * speed * 0.05;
			if (jeltz->button("Left Ctrl"))
				msensitivity *= 0.1;
			if (jeltz->button("Left Shift"))
				msensitivity *= 10.0;
			camera.zoom(-move.y * (camera.getZoom() + 1.0) * msensitivity);
			if (camera.getZoom() == 0.0 && move.y > 0.0 && jeltz->button("Left Shift"))
				camera.move(camera.toVec() * 0.1f);
			moved = true;
		}
	}
	
	//smooth snap to orbit position
	if (interpolating)
	{
		if (moved)
		{
			//remove roll and stop interpolating
			camera.lock(true);
			camera.setRotation(camera.getEuler() * vec3f(1,1,0));
			interpolating = false;
		}
		else
		{
			interpRatio += dt * msnapSpeed;
			if (interpRatio >= 1.0f)
			{
				interpolating = false;
				interpRatio = 1.0f;
				camera.lock(true);
				camera.zoomAt(camera.getPosition(), orbit);
				vec3f euler = camera.getEuler();
				if (myabs(euler.x) > pi*0.49f)
				{
					euler.y = (Quat(pi*0.5f,vec3f(1,0,0))*interpTo).euler().z;
					printf("Fixing final yaw: %.2fdeg\n", euler.y * 180.0f/pi);
				}
				camera.setRotation(euler);
				//camera.setRotation(camera.getOffset().inverse() * camera.getRotation());
				//camera.setRotation(interpTo);
				//camera.setZoom((camera.getPosition() - orbit).size());
				//camera.setPosition(orbit);
				//camera.setOffset(Quat::identity());
			}
			else
			{
				float smoothRatio = 1.0f - (cos(pi*interpRatio)*0.5f+0.5f);
				//float smoothRatio = mysign(interpRatio*2.0f-1.0f)*pow(myabs(interpRatio*2.0f-1.0f),0.5)*0.5+0.5;
				interpPos = interpFrom.slerp(interpTo, smoothRatio);
				camera.setRotation(interpPos);
			}
			//camera.lookAt(camera.getPosition(), camera.getPosition() + interpPos * vec3f(0,0,-1));
			//camera.setOffset(interpPos);
			moved = true;
		}
	}
	
	//only regen matrix if the camera has changed
	if (moved)
		camera.regenCamera();
		
	//update projection aspect ratio if window resized
	if (jeltz->resized())
	{
		camera.setAspectRatio(jeltz->winSize().x/(float)jeltz->winSize().y);
		camera.regenProjection();
	}
}
void JeltzFly::loadCamera(std::string filename)
{
	vec3f pos;
	vec3f rot;
	float z;
	ifstream ifile(filename.c_str());
	if (ifile.is_open())
	{
		ifile >> pos.x >> pos.y >> pos.z;
		ifile >> rot.x >> rot.y >> rot.z;
		ifile >> z;
		ifile.close();
		camera.setZoom(z);
		camera.setRotation(rot);
		camera.setPosition(pos);
		camera.regenCamera();
	}
}
void JeltzFly::saveCamera(std::string filename)
{
	vec3f pos = camera.getPosition();
	vec3f rot = camera.getEuler();
	float z = camera.getZoom();
	ofstream ofile(filename.c_str());
	ofile << pos.x << " " << pos.y << " " << pos.z << endl;
	ofile << rot.x << " " << rot.y << " " << rot.z << endl;
	ofile << z << endl;
	ofile.close();
	printf("Saved Camera\n");
}
void JeltzFly::uploadCamera(Shader* shader)
{
	if (shader->active == shader)
	{
		shader->set("normalMat", mat33(camera.getTransform().transpose()));
		shader->set("modelviewMat", camera.getInverse());
		shader->set("projectionMat", camera.getProjection());
	}
	else if (!shader->error())
		printf("Please \"%s\".use() before .uploadCamera()\n", shader->name().c_str());
}
void JeltzFly::uploadCamera()
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(camera.getProjection().m);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(camera.getInverse().m);
}
void JeltzFly::uploadOrtho()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, jeltz->winSize().x, 0, jeltz->winSize().y, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, jeltz->winSize().y, 0);
	glScalef(1, -1, 1);
}
