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

### change these. TARGET with .a and .so are handled automatically. point DEP_LIBS to dependent libraries for recursive compilation
#TARGET=pyarlib$(ASFX).a
#DEP_LIBS=mesh/lib3ds/lib3ds$(ASFX).a mesh/openctm/libopenctm$(ASFX).a mesh/simpleobj/libsimpleobj$(ASFX).a
#CFLAGS= -Wno-unused-parameter -Wno-unused-but-set-variable `pkg-config freetype2 --cflags` -std=c++11 -Wall -Wextra -D_GNU_SOURCE -Wfatal-errors
#LIBRARIES= -lopenal -lrt -lGLU -lGLEW `pkg-config freetype2 --libs` -lm -lpthread -lpng -lz -lGL `sdl2-config --libs`
#CC=g++
#SOURCE_SEARCH=
#EXCLUDE_SOURCE=
#PRECOMPILED_HEADER=prec.h
#TMP=.build

#automatic executable/library handling
TARGET_SUFFIX=$(suffix $(TARGET))
ifeq ($(TARGET_SUFFIX),.a)
  LD=ar
  LFLAGS= -rcs
  LFLAGS_DEPS=$(DEP_LIBS_R)
else ifeq ($(TARGET_SUFFIX),.so)
  LD=g++
  LFLAGS= -Wl,--no-undefined -shared -fPIC $(LIBRARIES)
  LFLAGS_DEPS= -Wl,--whole-archive $(DEP_LIBS_R) -Wl,--no-whole-archive
else
  LD=g++
  LFLAGS= $(LIBRARIES)
  LFLAGS_DEPS=$(DEP_LIBS_R)
endif

#internal stuff
#NOTE: if the makefile hangs, last time it was during $(DEP_LIBS_R_REL) initialization
SHELL=bash
ASFX:=$(shell getconf LONG_BIT | grep 64)
DEP_LIB_DIRS=$(dir $(DEP_LIBS))
DEP_LIBS_R_REL:=$(DEP_LIBS) $(foreach deplibdir, $(DEP_LIB_DIRS), $(foreach deplib, $(shell make echodeps --no-print-directory -C $(deplibdir) 2>/dev/null ), $(join $(deplibdir), $(deplib))))
DEP_LIBS_R:=$(shell echo $(foreach deplib, $(DEP_LIBS_R_REL), $(abspath $(deplib))) | tr ' ' '\n' | awk '!a[$$0]++')
NEWER_DEP_LIBS:=$(foreach deplibdir, $(DEP_LIB_DIRS), $(shell $(MAKE) -q --no-print-directory -C $(deplibdir) 2>/dev/null || echo $(deplibdir) ) )
NEWER_DEP_LIB_DIRS:=$(dir $(NEWER_DEP_LIBS))
CFLAGS+=$(DEP_LIB_DIRS:%=-I%/../)
SOURCE_C:=$(filter-out $(EXCLUDE_SOURCE),$(wildcard $(SOURCE_SEARCH)*.c))
OBJECTS_C:=$(filter %.o,$(SOURCE_C:%.c=$(TMP)/%.o))
SOURCE_CPP:=$(filter-out $(EXCLUDE_SOURCE),$(wildcard $(SOURCE_SEARCH)*.cpp))
OBJECTS_CPP:=$(filter %.o,$(SOURCE_CPP:%.cpp=$(TMP)/%.o))
SOURCE:=$(SOURCE_C) $(SOURCE_CPP)
OBJECTS:=$(OBJECTS_C) $(OBJECTS_CPP)
PCH_OBJ:=$(filter %.h.gch,$(PRECOMPILED_HEADER:%.h=$(TMP)/%.h.gch))
DEPS:=$(filter %.d,$(SOURCE_C:%.c=$(TMP)/%.d) $(SOURCE_CPP:%.cpp=$(TMP)/%.d))
CFLAGS_R?=
CFLAG_OPTS=$(CFLAGS_R)
CFLAGS+=$(CFLAG_OPTS)
INDENT_COUNT?=
export INDENT_COUNT+="+"
INDENT=$(shell echo $(INDENT_COUNT) | sed 's/^ *+//' | sed 's/ *+/    /g')

ifneq ($(PRECOMPILED_HEADER),)
  PCH_FLAGS= -I$(TMP) -include $(PCH_OBJ:%.h.gch=%.h) #have to force gcc to find the precompiled header in TMP dir
  PCH_WARN_FILES:=$(shell grep -L '\#include "$(PRECOMPILED_HEADER)"' $(SOURCE))
  ifneq ($(PCH_WARN_FILES),)
    $(warning The following files do not include PCH($(PRECOMPILED_HEADER)): $(PCH_WARN_FILES))
  endif
endif

.PHONY: all debug prof opt clean cleaner echodeps $(NEWER_DEP_LIB_DIRS)

debug: CFLAG_OPTS+= -g
debug: MAKE_OPTION=debug
debug: all

prof: CFLAG_OPTS+= -pg
prof: MAKE_OPTION=prof
prof: all

opt: CFLAG_OPTS+= -O3
opt: MAKE_OPTION=opt
opt: all

all: $(TMP) $(TARGET)

#prints a list of library dependencies recursively
echodeps:
	@echo $(foreach deplib, $(DEP_LIBS_R), $(shell python -c "import os.path; print os.path.relpath(\"$(deplib)\")"))

-include $(DEPS)

$(TMP):
	@[[ ! -d $(TMP) ]] && mkdir -p $(TMP) && echo Creating $(TMP) || true

#linking/archiving the target
$(TARGET): $(NEWER_DEP_LIB_DIRS) $(DEP_LIBS) $(PCH_OBJ) $(OBJECTS)
	@echo -e "$(INDENT)\e[1mlinking $(TARGET)\e[0m ($(LD))"
	@$(LD) -o $@ $(OBJECTS) $(LFLAGS_DEPS) $(LFLAGS)

#library dependencies
$(NEWER_DEP_LIB_DIRS):
	@echo "$(INDENT)"+$(dir $@)
	@$(MAKE) $(MAKE_OPTION) --no-print-directory -C $(dir $@)
	@echo "$(INDENT)"-$(dir $@)

#compiling source to objects and generating deps
$(TMP)/%.o : %.cpp
	@echo -e "$(INDENT)compiling \e[1m$@\e[0m $(CFLAG_OPTS)" `[[ "$$CCACHE_DISABLE" -eq "1" ]] && echo no ccache`
	@$(CC) $(PCH_FLAGS) $(CFLAGS) -MMD -MP -c $< -o $@ 2>&1
	
$(TMP)/%.o : %.c
	@echo -e "$(INDENT)compiling \e[1m$@\e[0m $(CFLAG_OPTS)"
	@$(CC) $(PCH_FLAGS) $(CFLAGS) -MMD -MP -c $< -o $@ 2>&1
	
$(TMP)/%.h.gch: %.h
	@echo -e "$(INDENT)compiling \e[1;31m$@\e[0m $(CFLAG_OPTS)"
	@export CCACHE_DISABLE=1 && $(CC) $(CFLAGS) -MMD -MP -c $< -o $@ 2>&1

#object dependencies
./main.o: main.cpp dof.h rays.h
./rays.o: rays.cpp rays.h 
./dof.o: dof.cpp dof.h 

#clean and cleaner
clean:
	@echo "$(INDENT)"cleaning $(TARGET)
	@rm -f $(PCH_OBJ) $(OBJECTS) $(DEPS) #$(TARGET)

cleaner: clean
	@$(foreach deplibdir, $(DEP_LIB_DIRS), echo "$(INDENT)"+$(deplibdir) ; $(MAKE) cleaner --no-print-directory -C $(deplibdir) ; echo "$(INDENT)"-$(deplibdir) )











