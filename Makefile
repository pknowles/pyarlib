#My all-in-one makefile. By Pyarelal Knowles.

#make commands:
#	<default> - debug
#	debug - for gdb
#	prof - for gprof
#	opt - optimizations
#	clean - remove intermediates
#	cleaner - clean + recurse into DEP_LIBS
#	echodeps - print DEP_LIBS and child DEP_LIBS

#TODO: automatically include LIBRARIES specified by child makefiles

#change these. TARGET with .a and .so are handled automatically. point DEP_LIBS to dependent libraries for recursive compilation
TARGET=pyarlib$(ASFX).a
DEP_LIBS=mesh/lib3ds/lib3ds$(ASFX).a mesh/openctm/libopenctm$(ASFX).a mesh/simpleobj/libsimpleobj$(ASFX).a
CFLAGS= -Wno-unused-parameter -Wno-unused-but-set-variable `pkg-config freetype2 --cflags` -std=c++11 -Wall -Wextra -D_GNU_SOURCE -Wfatal-errors
LIBRARIES= -lopenal -lrt -lGLU -lGLEW `pkg-config freetype2 --libs` -lm -lpthread -lpng -lz -lGL `sdl2-config --libs`
CC=g++
SOURCE_SEARCH=
EXCLUDE_SOURCE=
PRECOMPILED_HEADER=prec.h
TMP=.build

include recursive.make
