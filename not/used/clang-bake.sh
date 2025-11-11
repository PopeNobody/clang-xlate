# 1. Install LLVM/Clang development packages
#    Ubuntu: sudo apt install libclang-dev llvm-dev
#    macOS: brew install llvm
set -xe
die() {
  echo "failed to build $@"
  rm -f "$@"
  exit 1
};
# --------------------------------------------------------------
# 1. Pre-processor flags  (clang++ -E)
# --------------------------------------------------------------
cfg-pp() {
  # --cppflags is the official name for the *C-pre-processor* flags
  llvm-config --cppflags
  # we always want C++17, optimisation and debug info
  printf ' -std=c++17 -O2 -g'
}

# --------------------------------------------------------------
# 2. Compiler flags  (clang++ -c)
# --------------------------------------------------------------
cfg-cc() {
  # The same flags as the pre-processor are required for the .ii → .o step
  cfg-pp
}

# --------------------------------------------------------------
# 3. Linker flags  (clang++ … -o clang-bake)
# --------------------------------------------------------------
cfg-ld() {
  # --ldflags   → linker search paths
  # --system-libs → -lc, -lpthread, … that LLVM needs
  # --libs      → -lLLVMCore -lLLVMSupport … (the *real* libs)
  llvm-config --ldflags --system-libs --libs
  # The three Clang-Tooling libraries that are *not* returned by --libs
  printf ' -lclangTooling -lclangFrontend -lclangDriver'
}
pp() {
  clang++ $(cfg-pp) -g clang-bake.cpp -E -o clang-bake.i ||
    die "clang-bake.i"
}
# 2. Compile (adjust path to your llvm-config if needed)
cc() {
  clang++ $(cfg-cc) clang-bake.i -c -o clang-bake.o ||
    die "clang-bake.o"
}
ln() {
  clang++ clang-bake.o $(cfg-ld) \
    -o clang-bake || die "clang-bake"
};

if test "$1" == "--config"; then
  pp-cfg
  cc-cfg
  ld-cfg
else
  if ! test -e clang-bake.i || test clang-bake.cpp -nt clang-bake.i ; then
    pp;
  fi
  if ! test -e clang-bake.o || test clang-bake.i -nt clang-bake.o ; then
    cc;
  fi

  if ! test -e clang-bake || test clang-bake.o -nt clang-bake ; then
    ln
  fi
fi
