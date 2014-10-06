/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */
#version 150

out vec4 fragColour;

#include "phong.glsl"

void main()
{
	fragColour = phong();
}

