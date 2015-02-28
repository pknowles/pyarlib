#version 420

out vec4 fragColour;

#define DRBUG_LINES 1

#if DRBUG_LINES

in vec4 colour;

void main()
{
	fragColour = colour;
}
#endif
