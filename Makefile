all:
test: all
	./bin/macro-obs ./bin/macro-obs.cc 2>&1

# build_local.sh - Build with local clang
MAKEFLAGS+=rR
CXX:=clang++
CC:=clang
.PHONY: all FORCE
all: 

my-libs:= lib/libutil.a
%: %.new 
	cp $< $@

c++/src:=$(wildcard bin/*.cc)
c++/bin:=$(c++/src:.cc=)
c++/lib:=$(wildcard lib/*.cc)
c++/obj:=$(c++/src:.cc=.cc.oo)

all: $(c++/bin)
obj: $(c++/obj)

%: %.cc.oo etc/ldflags etc/libs $(my-libs)
	$(CXX) @etc/ldflags -o $@ $< @etc/libs

%.cc.oo: %.cc etc/cxxflags
	$(CXX) -c @etc/cxxflags -o $@ $<
#    
#
lib/libutil.a: $(c++/lib:.cc=.cc.oo)
	ar r $@ $(c++/lib:.cc=.cc.oo)
	llvm-ranlib $@

.PRECIOUS: */*.oo */*.a

etc/cxxflags:
etc/ldflags:
etc/libs:
clang-cpp-libs:= -lclang-cpp -lclang

etc/libs:
	printf "%s\n" $(shell llvm-config-19 --libs) $(clang-cpp-libs) $(my-libs) > $@
#    etc/ldflags:
#    	printf "%s\n" $(shell llvm-config-19 --ldflags) > $@ -v
#    etc/cxxflags:
#    	printf "%s\n" $(shell llvm-config-19 --cxxflags) > $@
Makefile:;

