
all: vec.h vec.cpp
	cp vec.h vec.cpp ..

vec.h: vec.h.gen
	./gencode.py vec.h.gen
	
vec.cpp: vec.cpp.gen
	./gencode.py vec.cpp.gen

clean:
	rm -f vec.h vec.cpp
