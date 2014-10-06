/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */


#ifndef QI_IMG_H
#define QI_IMG_H

#include <string>
#ifdef _WIN32
#include <memory>
#else
#include <tr1/memory>
#endif

#include "util.h"

namespace QI
{
	struct RCByteArray : public std::tr1::shared_ptr<unsigned char>
	{
		RCByteArray() : std::tr1::shared_ptr<unsigned char>() {}
		RCByteArray(unsigned char* o) : std::tr1::shared_ptr<unsigned char>(o, my_array_deleter<unsigned char>()) {}
		void operator()(unsigned char* todelete) const {delete[] todelete;}
		operator unsigned char*() {return get();}
	};
	
	struct Image
	{
		bool repeat;
		bool mipmap;
		bool nearest;
		int anisotropy;
		int channels;
		int width, height;
		RCByteArray data;
		Image();
		Image(const Image& other);
		virtual ~Image();
		virtual bool loadImage(std::string filename);
		virtual bool saveImage(std::string filename);
		void generateNoise();
		void generateChecker(unsigned char a = 0, unsigned char b = 255);
		unsigned int bufferTexture(); //returns new GL texture
		void bufferTexture(unsigned int object, unsigned long target = -1); //buffers to object
		void readTexture(unsigned int id);
		void resize(int w, int h, int nchannels = 0); //clears old image!
		void genHostMipmap(const Image& src); //generates an image based on the src image
	};
}

#endif
