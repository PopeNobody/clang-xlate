MAKEFLAGS=-rR
all:
test: all
	./bin/macro-obs ./bin/macro-obs.cc 2>&1

# build_local.sh - Build with local clang
SHELL:=/bin/bash -c >out 2>&1
CXX:=clang++
CC:=clang
.PHONY: all FORCE
all: 

my-libs:= 
#lib/libutil.a

# Remove the problematic catch-all pattern rule
# %: %.new 
#	cp $< $@

c++/ign:=$(file <.make-ignore)
c++/src:=$(wildcard bin/*.cc)
c++/src:=$(filter-out $(c++/ign),$(c++/src))
c++/bin:=$(patsubst %.cc,%,$(c++/src))
c++/lib:=$(wildcard lib/*.cc)
c++/obj:=$(c++/src:.cc=.cc.oo)
c++/lst:=$(c++/src:.cc=.cc.lst)

clean:=$(foreach s,bin lib obj,$(c++/$s))
clean:=$(wildcard bin/*.cc.* lib/*.cc.*)
clean:=$(filter-out bin/*.*,$(wildcard bin/*))
clean:=$(sort $(clean))
#    clean:
#    	$(if $(clean),rm -vf $(clean))

all: $(c++/bin)
obj: #$(c++/obj)

%: %.cc.oo etc/ldflags etc/libs $(my-libs)
	$(CXX) @etc/ldflags -o $@ $< @etc/libs
#    	sort .gitignore -u <(echo $@) -o .gitignore.new
#    	cmp .gitignore .gitignore.new || mv -v .gitignore.new .gitignore
#    	rm -vf .gitignore.new

%.cc.oo: %.cc etc/cxxflags
	$(CXX) -c @etc/cxxflags -o $@ $<
    

#    lib/libutil.a: $(c++/lib:.cc=.cc.oo)
#    	ar r $@ $(c++/lib:.cc=.cc.oo)
#    	llvm-ranlib $@

.PRECIOUS: */*.oo */*.a $(c++/exes)

etc/cxxflags:
	@mkdir -p etc
	llvm-config-19 --cxxflags > $@

etc/ldflags: ;
etc/libs: ;
clang-cpp-libs:= -lclang-cpp -lclang

#    etc/libs:
#    	printf "%s\n" $(shell llvm-config-19 --libs) $(clang-cpp-libs) $(my-libs) > $@
#    etc/ldflags:
#    	printf "%s\n" $(shell llvm-config-19 --ldflags) > $@ -v
#    etc/cxxflags:
#    	printf "%s\n" $(shell llvm-config-19 --cxxflags) > $@
Makefile:;

