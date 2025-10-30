# C/C++ Declaration Extractor

A tool that uses libclang to extract and display all declarations/definitions from C/C++ source files in a concise format. Particularly useful for analyzing code patterns like handle vs. pointer declarations.

## Features

- **Extracts all declarations**: functions, variables, typedefs, structs, unions, enums, fields
- **Handles broken code**: Uses lenient parsing options to work with incomplete/malformed source
- **Concise output**: Shows declarations without implementations (e.g., `int foo(void);` not the full function body)
- **Definitions annotated**: Marks whether each item is a declaration or definition
- **Optional macro support**: Can include preprocessor macro definitions
- **Location tracking**: Shows line:column for each declaration

## Installation

```bash
pip install libclang --break-system-packages
```

## Usage

### Basic usage
```bash
./decl_extractor.py source.c
```

### Include macro definitions
```bash
./decl_extractor.py source.c -m
```

### Show only definitions (not declarations)
```bash
./decl_extractor.py source.c -d
```

### Pass compiler flags to clang
Use `--` to separate tool arguments from clang arguments:

```bash
# Add include path
./decl_extractor.py source.c -- -I./include -I/usr/local/include

# Specify C standard
./decl_extractor.py source.c -- -std=c99

# Multiple flags
./decl_extractor.py source.c -m -- -I./include -std=gnu11 -DDEBUG
```

### Common clang flags for bash source
Bash is typically compiled as C with GNU extensions:
```bash
./decl_extractor.py bashfile.c -- -std=gnu89 -I. -I./include -I./lib
```

## Output Format

Each declaration is shown as:
```
<declaration>; // <declaration|definition> at <line>:<column>
```

Examples:
```
int process_data(struct buffer * buf);  // declaration at 40:5
struct buffer;  // definition at 13:8
void * global_handle;  // declaration at 35:8
typedef void * handle_t;  // declaration at 8:15
```

## Example

Given this C code:
```c
#define HANDLE void*

typedef void* handle_t;
typedef struct buffer* buffer_handle;

struct buffer {
    char* data;
    size_t size;
};

HANDLE global_handle;
handle_t process_handle;

int process_data(struct buffer* buf);
void cleanup_resources(HANDLE h, int flags) {
    free(h);
}
```

Running:
```bash
./decl_extractor.py example.c -- -I/usr/lib/gcc/x86_64-linux-gnu/13/include
```

Outputs:
```
=== Declarations from example.c ===

typedef void * handle_t;  // declaration at 8:15
typedef struct buffer * buffer_handle;  // declaration at 9:24
struct buffer;  // definition at 13:8
char * data;  // declaration at 14:11
size_t size;  // declaration at 15:12
void * global_handle;  // declaration at 35:8
handle_t process_handle;  // declaration at 37:10
int process_data(struct buffer * buf);  // declaration at 40:5
void cleanup_resources(void * h, int flags);  // declaration at 53:6

=== Total: 9 items ===
```

## Finding Handle vs Pointer Patterns

To find where handles are declared as values instead of pointers:

1. Run the extractor on your source files
2. Look for patterns like:
   - `HANDLE foo;` (should be `HANDLE *foo;`?)
   - `handle_t bar;` (should be `handle_t *bar;`?)
3. Compare against:
   - `struct foo *ptr;` (correct pointer style)

### Filtering output

```bash
# Find all HANDLE declarations
./decl_extractor.py source.c -- -I./include | grep HANDLE

# Find all typedef declarations
./decl_extractor.py source.c | grep '^typedef'

# Find all definitions (not just declarations)
./decl_extractor.py source.c -d
```

## Handling Broken/Incomplete Code

The tool is configured to be lenient with parsing errors:

- **PARSE_INCOMPLETE**: Handles incomplete code
- **PARSE_SKIP_FUNCTION_BODIES**: Skips function implementations (faster, focuses on declarations)
- **Error limit disabled**: Shows all errors but continues parsing

Diagnostics (errors/warnings) are printed to stderr, so you can separate them:
```bash
# Only see declarations
./decl_extractor.py broken.c 2>/dev/null

# Only see errors
./decl_extractor.py broken.c 1>/dev/null
```

## Use Cases

### 1. Analyzing handle patterns in bash
```bash
./decl_extractor.py variables.c -- -I. -I./include -std=gnu89 | grep -i handle
```

### 2. Finding all typedefs
```bash
./decl_extractor.py *.c -- -I./include | grep '^typedef'
```

### 3. Listing all function signatures
```bash
./decl_extractor.py source.c | grep '(.*);.*// declaration'
```

### 4. Comparing declarations vs definitions
```bash
# Declarations only (prototypes)
./decl_extractor.py source.c | grep '// declaration'

# Definitions only (implementations)
./decl_extractor.py source.c | grep '// definition'
```

## Limitations

- Macro expansion is limited (shows macro definition, not expanded usage)
- Template code (C++) may require additional flags
- Some complex preprocessor constructs may not parse correctly
- For heavily broken code, clang may fail to produce any AST

## Advanced: Finding Specific Type Usage

To find all places a specific type is used, you can combine with grep:

```bash
# Find all HANDLE variables
./decl_extractor.py *.c -- -I./include | grep 'HANDLE' | grep -v 'typedef'

# Find function pointers
./decl_extractor.py source.c | grep 'typedef.*\*.*(.*)' 

# Find all struct buffer usage
./decl_extractor.py source.c | grep 'struct buffer'
```

## Next Steps

For more sophisticated analysis (like tracking where macros are expanded), consider:
1. Using clang's preprocessor output: `clang -E source.c`
2. Using compiler-explorer or clang AST dump: `clang -Xclang -ast-dump source.c`
3. Writing a custom clang tool using the visitor pattern for type replacement

## License

Free to use and modify.
