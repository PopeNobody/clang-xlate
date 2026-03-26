# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**clang-xlate** is a C-to-C++ code translator built using libclang and LLVM. The project provides tools to analyze C code patterns and will eventually perform automated C-to-C++ translation. The primary use case is translating the bash shell from C to C++ (the "cash" project).

## Build System

The project uses a custom Makefile that dynamically generates configuration files from `llvm-config-19`:

```bash
make              # Build all binaries in bin/
make test         # Run test (executes macro-obs on itself)
make clean        # Clean build artifacts (if implemented)
```

### Build Process

1. **Configuration Generation**: First run creates `etc/cxxflags`, `etc/ldflags`, and `etc/libs` from `llvm-config-19`
2. **Compilation**: C++ sources compiled with LLVM/Clang flags
3. **Linking**: Binaries linked against libclang-cpp and libclang

### Build Artifacts

- `bin/*.cc` → `bin/*.cc.oo` → `bin/*` (executables)
- `lib/*.cc` → `lib/*.cc.oo` → `lib/libutil.a` (static library)
- Generated config: `etc/cxxflags`, `etc/ldflags`, `etc/libs`

## Key Tools

### 1. `bin/decls` - Declaration Extractor
C++ implementation using libclang's libtooling API to extract all declarations from C/C++ source files.

**Usage:**
```bash
./bin/decls source.c                    # Extract declarations
./bin/decls source.c -- -I./include     # With compiler flags
./bin/decls source.c -d                 # Definitions only
./bin/decls source.c -m                 # Include macros
```

**For bash source code:**
```bash
./bin/decls bashfile.c -u -- -x c -std=gnu89 -I./include -I./lib -D_GNU_SOURCE
```

The `-u` flag deduplicates `typedef struct` patterns which appear twice in the AST.

### 2. `bin/macro-obs` - Macro Observer
Reports all preprocessor constructs (defines, undefs, conditionals) to stderr while passing through source to stdout.

**Usage:**
```bash
./bin/macro-obs source.cc > output 2> macros.log
```

### 3. Python Scripts (Exposition Only)
The `scr/*.py` scripts are for documentation/demonstration. The C++ implementations are the primary tools.

- `scr/decl_extractor.py` - Python version of declaration extractor
- `scr/normalize_typedef.py` - Normalizes `typedef struct` patterns

**Setup:** `pip install libclang --break-system-packages`

## Project Structure

```
bin/          C++ source for executable tools
  decls.cc           Declaration extractor (libtooling)
  macro-obs.cc       Macro preprocessor observer
  *.cc.oo           Object files
  decls, macro-obs  Compiled binaries

lib/          Library code
  qx.cc             Utility library source
  libutil.a         Compiled static library

scr/          Scripts (exposition only - Python)
  decl_extractor.py
  normalize_typedef.py

etc/          Auto-generated build configuration
  cxxflags          LLVM C++ compiler flags
  ldflags           LLVM linker flags
  libs              Required libraries list

doc/          Documentation
  README.md         Main docs (symlinked to ../README.md)
  TYPEDEF_STRUCT.md Explains C typedef struct pattern issues
  README_CPP.md     C++ implementation notes
```

## Architecture & Patterns

### LibClang Integration

Both tools use libclang's C++ API to parse C/C++ code without requiring the code to compile cleanly:

- **Parse Options**: `CXTranslationUnit_SkipFunctionBodies` for performance
- **Resilience**: Continues parsing even with errors
- **AST Visiting**: Uses `RecursiveASTVisitor` pattern to walk the AST

### The typedef struct Pattern

Bash code uses C89 idiom: `typedef struct foo { ... } FOO;`

This creates duplicates in libclang's AST:
1. The struct definition itself
2. The same struct as part of the typedef

Use `-u` flag on `decls` to deduplicate when analyzing C code with this pattern.

### Makefile Pattern

The Makefile uses file-based configuration stored in `etc/`:
- Compiler/linker flags in separate files
- `@etc/cxxflags` syntax to read flags from file
- Auto-generated from `llvm-config-19` on first build

## Common Development Tasks

### Adding a New Tool

1. Create `bin/newtool.cc`
2. Add to `c++/src` glob pattern in Makefile (automatic via wildcard)
3. Run `make` - will compile and link automatically

### Modifying Library Code

1. Edit files in `lib/*.cc`
2. Library automatically rebuilt into `lib/libutil.a`
3. All binaries automatically relinked

### Regenerating Build Configuration

```bash
rm etc/cxxflags etc/ldflags etc/libs
make              # Will regenerate from llvm-config-19
```

## Dependencies

- **LLVM/Clang 19**: Required for libclang-cpp and libclang
- **llvm-config-19**: Must be in PATH for build system
- **C++17**: Required language standard

Install on Ubuntu/Debian:
```bash
sudo apt-get install llvm-19-dev libclang-19-dev clang-19
```

## Working with Bash Source Code

Bash is written in C89/C90 with GNU extensions. To analyze bash code:

```bash
# Standard flags for bash source
./bin/decls bashfile.c -u -- \
  -x c \
  -std=gnu89 \
  -I. -I./include -I./lib \
  -D_GNU_SOURCE \
  -Wno-implicit-function-declaration
```

Key flags:
- `-u` - Deduplicate typedef struct patterns
- `-x c` - Force C mode (not C++)
- `-std=gnu89` - C89 with GNU extensions
- `-D_GNU_SOURCE` - Enable GNU libc features

## Future Direction

This project is building toward a full C-to-C++ translator that will:
1. Parse C source with libclang
2. Analyze type patterns and code structure
3. Generate equivalent C++ code
4. Apply to bash shell source code to create "++bash" (cash)

Current tools are foundational components for this larger goal.
