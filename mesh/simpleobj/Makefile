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
TARGET=libsimpleobj$(ASFX).a
DEP_LIBS=
CFLAGS=-Wno-unused-parameter -Wno-unused-but-set-variable -Wall -Wextra -D_GNU_SOURCE -Wfatal-errors
LIBRARIES=
CC=g++
SOURCE_SEARCH=
EXCLUDE_SOURCE=
PRECOMPILED_HEADER=
TMP=.build

include ../../recursive.make

