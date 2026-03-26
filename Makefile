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
src:=$(wildcard */*.cc)
aun:=$(src:.cc=)
bun:=$(filter bin/%,$(aun))
tun:=$(filter tst/%,$(aun))
lun:=$(filter-out $(bun) $(tun),$(aun))
#$(foreach x,aun bun tun lun src,$(warning $x:=$($x)))
$(foreach d,$(sort $(dir $(lun))),$(eval lib+=$(d:/=)))

libs:=util/libutil.a
all: $(libs)
%: %.cc.oo etc/ldflags etc/libs $(libs)
	$(CXX) @etc/ldflags -o $@ $< @etc/libs

%.cc.oo: %.cc etc/cxxflags
	$(CXX) -c @etc/cxxflags -o $@ $<

define deflib
$1:=$$(filter $1/,$(aun))
$1/lib$1.a: 
	ar r $@ $$($1:=$($1))
	llvm-ranlib $@

$(eval $($$1))
endef
wareval=$(warning $1) $(eval $1)
xshow=$1:=$$($1)
show=$(warning $(call xshow,$1))
$(foreach l,$(lib),$(call wareval,$(call deflib,$l)))
$(warning $(call xshow,util))
.PRECIOUS: */*.oo */*.a
showx:=$(call xshow,util)
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

