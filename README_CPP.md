# C++ Declaration Extractor

Two C++ implementations for extracting declarations from C/C++ source code:

1. **decl_extractor_simple.cpp** - Simple version using libclang C API (recommended, easier to build)
2. **decl_extractor_tool.cpp** - Full libtooling version (more powerful but harder to build)

## Quick Start (Recommended: Simple Version)

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install libclang-dev

# Fedora/RHEL
sudo dnf install clang-devel

# macOS
brew install llvm
```

### Build
```bash
# Easy way: use the build script
./build.sh

# Manual compilation
g++ -std=c++17 -I/usr/lib/llvm-18/include decl_extractor_simple.cpp \
    -o decl_extractor -L/usr/lib/llvm-18/lib -lclang
```

### Usage
```bash
# Basic usage
./decl_extractor example.c

# With macros
./decl_extractor example.c -m

# With compiler flags
./decl_extractor source.c -- -I./include -std=c99

# Definitions only
./decl_extractor source.c -d
```

## Simple Version (decl_extractor_simple.cpp)

This version wraps libclang's C API in modern C++. It's much easier to compile than the full libtooling version.

**Pros:**
- Simple to compile (just need libclang-dev)
- Single source file
- Fast compilation
- Portable

**Cons:**
- Uses C API (slightly more verbose internally)
- Limited compared to full AST access

**Features:**
- Extracts all declarations: functions, variables, typedefs, structs, unions, enums
- Handles broken/incomplete code gracefully
- Optional macro support
- Clean C++ code with RAII and std::string

### Command Line Options
```
-m              Include macro definitions
-d              Show only definitions (not declarations)
-h, --help      Show help
--              Separator for clang arguments
```

### Examples

Find all function declarations:
```bash
./decl_extractor source.c | grep '(.*);'
```

Find all handle types:
```bash
./decl_extractor bashfile.c -- -I./include | grep -i handle
```

Find definitions only:
```bash
./decl_extractor source.c -d
```

## Libtooling Version (decl_extractor_tool.cpp)

Full libtooling implementation with RecursiveASTVisitor. More powerful but requires full LLVM/Clang development libraries.

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get install llvm-dev libclang-dev clang-tools

# You'll need the full LLVM/Clang development environment
```

### Build
```bash
# Using Makefile
make

# Manual (adjust for your LLVM version)
clang++ -std=c++17 $(llvm-config --cxxflags) decl_extractor_tool.cpp \
    -o decl_extractor $(llvm-config --ldflags --system-libs --libs support) \
    -lclangTooling -lclangFrontend -lclangDriver -lclangSerialization \
    -lclangParse -lclangSema -lclangAnalysis -lclangEdit \
    -lclangAST -lclangLex -lclangBasic
```

### Usage
```bash
# Same as simple version but uses LLVM's CommonOptionsParser
./decl_extractor example.c --

# With flags
./decl_extractor source.c -- -I./include -std=c99
```

**Pros:**
- Full AST access through RecursiveASTVisitor
- More extensible for complex analyses
- Better type information
- Native C++ API

**Cons:**
- Requires full LLVM/Clang development libs (large install)
- More complex build process
- Longer compilation time

## Comparison with Python Version

| Feature | Python | C++ Simple | C++ Libtooling |
|---------|--------|------------|----------------|
| Build complexity | pip install | Simple g++ | Complex linking |
| Dependencies | libclang Python | libclang C | Full LLVM/Clang |
| Performance | Good | Excellent | Excellent |
| Code clarity | High | Medium | High |
| Extensibility | Medium | Medium | High |
| Install size | ~50MB | ~50MB | ~500MB |

**When to use each:**
- **Python**: Prototyping, scripting, quick analysis
- **C++ Simple**: Embedding in C++ projects, performance critical, minimal dependencies
- **C++ Libtooling**: Complex AST transformations, integration with clang tools

## Implementation Notes

### How libclang Handles Broken Code

Both C++ versions use these flags for resilience:
```cpp
CXTranslationUnit_SkipFunctionBodies   // Faster, focus on declarations
CXTranslationUnit_DetailedPreprocessingRecord  // For macro support
```

The code continues parsing even with errors and shows diagnostics on stderr.

### Type Information

The simple version gets type strings via:
```cpp
CXType type = clang_getCursorType(cursor);
std::string typeStr = getTypeSpelling(type);
```

The libtooling version uses:
```cpp
QualType type = decl->getType();
std::string typeStr = type.getAsString();
```

### Visitor Pattern

**Simple version** uses C callback:
```cpp
clang_visitChildren(cursor, visitor, &data);
```

**Libtooling version** uses RecursiveASTVisitor:
```cpp
class DeclVisitor : public RecursiveASTVisitor<DeclVisitor> {
    bool VisitFunctionDecl(FunctionDecl *FD) { ... }
};
```

## Extending the Code

### Adding New Declaration Types

1. Add case to switch in `getDeclarationString()`
2. Handle in visitor callback/Visit method

Example for handling class templates:
```cpp
case CXCursor_ClassTemplate:
    return "template<...> class " + name + ";";
```

### Type Replacement

To add type replacement (like the Python type_replacer.py), you would:

1. Store original source locations and extents
2. Read source file
3. Build replacement map
4. Apply changes from end to start (to preserve offsets)

See the Python version for reference implementation.

### Performance Tips

- Use `CXTranslationUnit_SkipFunctionBodies` to skip function implementations
- For large codebases, process files in parallel
- Cache parsed translation units if analyzing multiple times

## Troubleshooting

### Cannot find clang-c/Index.h
```bash
# Find where it's installed
find /usr -name "Index.h" 2>/dev/null | grep clang

# Add to include path
g++ -I/usr/lib/llvm-XX/include ...
```

### Cannot find -lclang
```bash
# Find libclang
find /usr -name "libclang.so" 2>/dev/null

# Add to library path
g++ -L/usr/lib/llvm-XX/lib -lclang ...
```

### Runtime: libclang.so not found
```bash
# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/lib/llvm-18/lib:$LD_LIBRARY_PATH

# Or install to system location
sudo ldconfig
```

## License

Free to use and modify.
