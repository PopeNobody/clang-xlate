#!/bin/bash
# build_local.sh - Build with local clang

/opt/bin/clang++ -std=c++17 @etc/cxxflags $@ -o $<
   
/opt/bin/clang++ -std=c++17 \
    -Wl,--start-group \
    -Llib  \
    *.o \
    lib/*.a \
    -lz -lzstd \
    -lclang-cpp \
    -lclang \
    -Wl,--end-group \
