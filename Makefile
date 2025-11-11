all:
test: all
	./bin/macro-obs ./bin/macro-obs.cc 2>&1

# build_local.sh - Build with local clang
MAKEFLAGS+=rR
SHELL:=/bin/bash -c >out 2>&1
CXX:=clang++
CC:=clang
.PHONY: all FORCE
all: 

my-libs:= lib/libutil.a
%: %.new 
	cp $< $@

c++/ign:=$(file <.make-ignore)
c++/src:=$(wildcard bin/*.cc)
c++/src:=$(filter-out $(c++/ign),$(c++/src))
c++/bin:=$(c++/src:.cc=)
c++/lib:=$(wildcard lib/*.cc)
c++/obj:=$(c++/src:.cc=.cc.oo)
c++/lst:=$(c++/bin:.cc=.cc.lst)

clean:=$(foreach s,bin lib obj,$(c++/$s))
clean:=$(wildcard bin/*.cc.* lib/*.cc.*)
clean:=$(filter-out bin/*.*,$(wildcard bin/*))
clean:=$(sort $(clean))
$(error)
clean:
	$(if $(clean),rm -vf $(clean))

all: $(c++/bin)
obj: $(c++/obj)

%: %.cc.oo etc/ldflags etc/libs $(my-libs) $(spec_libs)
	$(CXX) @etc/ldflags -o $@ $< @etc/libs $(spec_libs)
	sort .gitignore -u <(echo $@) -o .gitignore.new
	cmp .gitignore .gitignore.new || mv -v .gitignore.new .gitignore
	rm -vf .gitignore.new

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
etc/ldflags:
	printf "%s\n" $(shell llvm-config-19 --ldflags) > $@ -v
etc/cxxflags:
	printf "%s\n" $(shell llvm-config-19 --cxxflags) > $@
Makefile:;

