#!/bin/bash
# Build script for C++ version of decl_extractor

set -e

echo "Building C++ version of decl_extractor..."

# Find LLVM installation
LLVM_CONFIG=$(which llvm-config 2>/dev/null || echo "")

if [ -n "$LLVM_CONFIG" ]; then
    echo "Found llvm-config: $LLVM_CONFIG"
    LLVM_CXXFLAGS=$($LLVM_CONFIG --cxxflags)
    LLVM_LDFLAGS=$($LLVM_CONFIG --ldflags)
    LLVM_LIBDIR=$($LLVM_CONFIG --libdir)
    
    echo "Compiling with llvm-config settings..."
    g++ -std=c++17 $LLVM_CXXFLAGS decl_extractor_simple.cpp -o decl_extractor \
        $LLVM_LDFLAGS -L$LLVM_LIBDIR -lclang
else
    echo "llvm-config not found, trying manual paths..."
    
    # Try to find clang headers
    CLANG_INCLUDE=""
    for dir in /usr/lib/llvm-*/include /usr/include; do
        if [ -f "$dir/clang-c/Index.h" ]; then
            CLANG_INCLUDE="$dir"
            break
        fi
    done
    
    if [ -z "$CLANG_INCLUDE" ]; then
        echo "Error: Could not find clang-c/Index.h"
        echo "Please install libclang-dev: sudo apt-get install libclang-dev"
        exit 1
    fi
    
    # Try to find libclang
    CLANG_LIB=""
    for dir in /usr/lib/llvm-*/lib /usr/lib/x86_64-linux-gnu /usr/lib; do
        if [ -f "$dir/libclang.so" ]; then
            CLANG_LIB="$dir"
            break
        fi
    done
    
    echo "Using include: $CLANG_INCLUDE"
    echo "Using lib: $CLANG_LIB"
    
    g++ -std=c++17 -I$CLANG_INCLUDE decl_extractor_simple.cpp -o decl_extractor \
        ${CLANG_LIB:+-L$CLANG_LIB} -lclang
fi

echo "Build successful! Binary: ./decl_extractor"
echo ""
echo "Test it with:"
echo "  ./decl_extractor example.c"
