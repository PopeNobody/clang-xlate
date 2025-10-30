#!/usr/bin/env python3
"""
Extract and display all declarations/definitions from C/C++ code using libclang.
Handles broken/incomplete code as gracefully as possible.
"""

import sys
import argparse
from pathlib import Path

try:
    import clang.cindex
    from clang.cindex import CursorKind, TypeKind
except ImportError:
    print("Error: clang python bindings not found.", file=sys.stderr)
    print("Install with: pip install libclang", file=sys.stderr)
    sys.exit(1)


class DeclExtractor:
    def __init__(self, show_macros=False):
        self.show_macros = show_macros
        self.declarations = []
        
    def get_type_string(self, cursor):
        """Get a readable type string for a cursor."""
        type_obj = cursor.type
        if type_obj.kind == TypeKind.INVALID:
            return "<invalid-type>"
        
        # Get the type spelling
        type_str = type_obj.spelling
        
        # For functions, build a better signature
        if cursor.kind == CursorKind.FUNCTION_DECL:
            result_type = type_obj.get_result().spelling
            params = []
            for arg in cursor.get_arguments():
                param_type = arg.type.spelling
                param_name = arg.spelling
                if param_name:
                    params.append(f"{param_type} {param_name}")
                else:
                    params.append(param_type)
            
            param_str = ", ".join(params) if params else "void"
            return f"{result_type} {cursor.spelling}({param_str})"
        
        return type_str
    
    def get_declaration_string(self, cursor):
        """Generate a concise declaration string."""
        kind = cursor.kind
        
        if kind == CursorKind.FUNCTION_DECL:
            type_str = self.get_type_string(cursor)
            return f"{type_str};"
        
        elif kind == CursorKind.VAR_DECL:
            type_str = cursor.type.spelling
            name = cursor.spelling
            return f"{type_str} {name};"
        
        elif kind == CursorKind.PARM_DECL:
            type_str = cursor.type.spelling
            name = cursor.spelling
            return f"{type_str} {name}"
        
        elif kind == CursorKind.TYPEDEF_DECL:
            underlying = cursor.underlying_typedef_type.spelling
            name = cursor.spelling
            return f"typedef {underlying} {name};"
        
        elif kind == CursorKind.STRUCT_DECL:
            name = cursor.spelling or "<anonymous>"
            return f"struct {name};"
        
        elif kind == CursorKind.UNION_DECL:
            name = cursor.spelling or "<anonymous>"
            return f"union {name};"
        
        elif kind == CursorKind.ENUM_DECL:
            name = cursor.spelling or "<anonymous>"
            return f"enum {name};"
        
        elif kind == CursorKind.FIELD_DECL:
            type_str = cursor.type.spelling
            name = cursor.spelling
            return f"{type_str} {name};"
        
        elif kind == CursorKind.ENUM_CONSTANT_DECL:
            name = cursor.spelling
            # Try to get the value
            tokens = list(cursor.get_tokens())
            if len(tokens) > 2 and tokens[1].spelling == '=':
                value = ''.join(t.spelling for t in tokens[2:])
                return f"{name} = {value}"
            return f"{name}"
        
        elif kind == CursorKind.MACRO_DEFINITION:
            if self.show_macros:
                tokens = list(cursor.get_tokens())
                if tokens:
                    macro_text = ' '.join(t.spelling for t in tokens)
                    return f"#define {macro_text}"
            return None
        
        else:
            # Generic fallback
            name = cursor.spelling or "<unnamed>"
            return f"{kind.name}: {name}"
    
    def is_definition(self, cursor):
        """Check if this cursor represents a definition (vs just declaration)."""
        if cursor.kind == CursorKind.FUNCTION_DECL:
            return cursor.is_definition()
        elif cursor.kind in (CursorKind.STRUCT_DECL, CursorKind.UNION_DECL, 
                            CursorKind.ENUM_DECL, CursorKind.CLASS_DECL):
            # Check if it has children (fields/members)
            return any(True for _ in cursor.get_children())
        return False
    
    def should_process(self, cursor, filename):
        """Determine if we should process this cursor."""
        # Skip if not in our main file
        if cursor.location.file:
            cursor_file = str(Path(cursor.location.file.name).resolve())
            if cursor_file != filename:
                return False
        
        # Skip invalid cursors
        if cursor.kind == CursorKind.INVALID_FILE:
            return False
        
        return True
    
    def extract_declarations(self, cursor, filename, depth=0):
        """Recursively extract declarations from the AST."""
        if not self.should_process(cursor, filename):
            return
        
        kind = cursor.kind
        
        # Kinds we want to capture
        interesting_kinds = {
            CursorKind.FUNCTION_DECL,
            CursorKind.VAR_DECL,
            CursorKind.TYPEDEF_DECL,
            CursorKind.STRUCT_DECL,
            CursorKind.UNION_DECL,
            CursorKind.ENUM_DECL,
            CursorKind.FIELD_DECL,
            CursorKind.ENUM_CONSTANT_DECL,
        }
        
        if self.show_macros:
            interesting_kinds.add(CursorKind.MACRO_DEFINITION)
        
        if kind in interesting_kinds:
            decl_str = self.get_declaration_string(cursor)
            if decl_str:
                is_def = self.is_definition(cursor)
                location = f"{cursor.location.line}:{cursor.location.column}"
                
                self.declarations.append({
                    'kind': kind.name,
                    'declaration': decl_str,
                    'is_definition': is_def,
                    'location': location,
                    'spelling': cursor.spelling,
                })
        
        # Recurse into children
        for child in cursor.get_children():
            self.extract_declarations(child, filename, depth + 1)


def parse_file(filename, show_macros=False, clang_args=None):
    """Parse a C/C++ file and extract declarations."""
    # Initialize libclang
    index = clang.cindex.Index.create()
    
    # Default arguments to handle broken code better
    default_args = [
        '-fsyntax-only',  # Don't generate code
        '-ferror-limit=0',  # Show all errors
        '-Wno-everything',  # Suppress warnings
    ]
    
    if clang_args:
        default_args.extend(clang_args)
    
    # Parse with options to be lenient
    tu = index.parse(
        filename,
        args=default_args,
        options=(
            clang.cindex.TranslationUnit.PARSE_INCOMPLETE |  # Handle incomplete code
            clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES |  # Skip function bodies
            clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
        )
    )
    
    # Show diagnostics (errors/warnings) if any
    if tu.diagnostics:
        print(f"=== Diagnostics for {filename} ===", file=sys.stderr)
        for diag in tu.diagnostics:
            print(f"  {diag}", file=sys.stderr)
        print("", file=sys.stderr)
    
    # Extract declarations
    extractor = DeclExtractor(show_macros=show_macros)
    extractor.extract_declarations(tu.cursor, str(Path(filename).resolve()))
    
    return extractor.declarations


def main():
    # Manual argument parsing to handle the -- separator properly
    argv = sys.argv[1:]
    
    filename = None
    show_macros = False
    definitions_only = False
    clang_args = []
    
    # Find the -- separator
    try:
        sep_idx = argv.index('--')
        main_args = argv[:sep_idx]
        clang_args = argv[sep_idx + 1:]
    except ValueError:
        main_args = argv
        clang_args = []
    
    # Parse main arguments
    parser = argparse.ArgumentParser(
        description='Extract declarations from C/C++ source files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s source.c
  %(prog)s source.c -m              # Include macro definitions
  %(prog)s source.c -- -I./include  # Pass include paths to clang
  %(prog)s source.c -- -std=c99     # Specify C standard
        """
    )
    parser.add_argument('filename', help='C/C++ source file to parse')
    parser.add_argument('-m', '--macros', action='store_true',
                       help='Include macro definitions')
    parser.add_argument('-d', '--definitions-only', action='store_true',
                       help='Show only definitions (not declarations)')
    
    args = parser.parse_args(main_args)
    
    if not Path(args.filename).exists():
        print(f"Error: File '{args.filename}' not found", file=sys.stderr)
        sys.exit(1)
    
    # Parse the file
    declarations = parse_file(args.filename, args.macros, clang_args)
    
    # Output results in GCC error format for vim -q
    for decl in declarations:
        if args.definitions_only and not decl['is_definition']:
            continue
        
        # Format: filename:line:column: declaration // definition|declaration
        suffix = "definition" if decl['is_definition'] else "declaration"
        print(f"{args.filename}:{decl['location']}: {decl['declaration']} // {suffix}")
    
    print(f"\n=== Total: {len(declarations)} items ===", file=sys.stderr)


if __name__ == '__main__':
    main()
