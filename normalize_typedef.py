#!/usr/bin/env python3
"""
Normalize typedef struct patterns for easier parsing.

Transforms:
  typedef struct foo { } FOO;  ->  struct foo { };
  typedef struct { } FOO;      ->  struct foo { };
"""

import re
import sys
import argparse

def normalize_typedef_struct(content):
    """
    Remove typedef struct patterns entirely.
    
    Pattern 1: typedef struct name { ... } anything;
      -> struct name { ... };
    
    Pattern 2: typedef struct { ... } NAME;  
      -> struct name { ... };  (using lowercase NAME as struct name)
    """
    
    # Pattern 1: typedef struct word_list { ... } struct word_list;
    # or: typedef struct word_list { ... } WORD_LIST;
    # -> struct word_list { ... };
    pattern1 = r'typedef\s+struct\s+(\w+)\s*(\{[^}]*\})\s*[^;]+;'
    
    def replace1(match):
        struct_name = match.group(1).lower()
        body = match.group(2)
        return f"struct {struct_name} {body};"
    
    content = re.sub(pattern1, replace1, content, flags=re.MULTILINE | re.DOTALL)
    
    # Pattern 2: typedef struct { ... } anything;
    # Anonymous struct
    pattern2 = r'typedef\s+struct\s*(\{[^}]*\})\s*([^;]+);'
    
    def replace2(match):
        body = match.group(1)
        # Extract the last word before semicolon - that's the typedef name
        typedef_part = match.group(2).strip()
        # Get last word (might be "struct something" after our replacement)
        words = typedef_part.split()
        typedef_name = words[-1] if words else 'anonymous'
        struct_name = typedef_name.lower()
        return f"struct {struct_name} {body};"
    
    content = re.sub(pattern2, replace2, content, flags=re.MULTILINE | re.DOTALL)
    
    return content


def normalize_forward_typedef(content):
    """
    Remove forward typedef declarations.
    
    typedef struct foo FOO;  ->  struct foo;
    """
    pattern = r'typedef\s+struct\s+(\w+)\s+\w+\s*;'
    
    def replace(match):
        struct_name = match.group(1).lower()
        return f"struct {struct_name};"
    
    return re.sub(pattern, replace, content)


def extract_typedef_map(content):
    """Extract mapping of TYPEDEF_NAME -> struct_name before normalization."""
    typedef_map = {}
    
    # Pattern 1: typedef struct name { ... } NAME;
    for match in re.finditer(r'typedef\s+struct\s+(\w+)\s*\{[^}]*\}\s*(\w+)\s*;', content, re.MULTILINE | re.DOTALL):
        struct_name = match.group(1).lower()
        typedef_name = match.group(2)
        typedef_map[typedef_name] = struct_name
    
    # Pattern 2: typedef struct { ... } NAME;
    for match in re.finditer(r'typedef\s+struct\s*\{[^}]*\}\s*(\w+)\s*;', content, re.MULTILINE | re.DOTALL):
        typedef_name = match.group(1)
        struct_name = typedef_name.lower()
        typedef_map[typedef_name] = struct_name
    
    # Pattern 3: typedef struct name NAME;
    for match in re.finditer(r'typedef\s+struct\s+(\w+)\s+(\w+)\s*;', content):
        struct_name = match.group(1).lower()
        typedef_name = match.group(2)
        typedef_map[typedef_name] = struct_name
    
    return typedef_map


def normalize_typedef_usage(content, typedef_map):
    """
    Replace usage of uppercase typedef names with lowercase struct names.
    
    WORD_LIST *ptr;  ->  struct word_list *ptr;
    """
    lines = content.split('\n')
    result_lines = []
    
    for line in lines:
        # Skip typedef lines themselves
        if re.match(r'^\s*typedef\s+struct', line):
            result_lines.append(line)
            continue
        
        # Replace typedef usage in this line
        for typedef_name, struct_name in typedef_map.items():
            # Only process uppercase typedefs
            if not (typedef_name.upper() == typedef_name and len(typedef_name) > 1):
                continue
            
            # Replace TYPENAME with struct typename (word boundaries)
            pattern = r'\b' + re.escape(typedef_name) + r'\b'
            replacement = f'struct {struct_name}'
            line = re.sub(pattern, replacement, line)
        
        result_lines.append(line)
    
    return '\n'.join(result_lines)


def main():
    parser = argparse.ArgumentParser(
        description='Normalize typedef struct patterns',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s input.c
  %(prog)s input.c -o output.c
  %(prog)s input.c | ./decl_extractor -
  
Transformations:
  typedef struct foo { } FOO;   ->  struct foo { };
  typedef struct { } FOO;       ->  struct foo { };  
  typedef struct foo FOO;       ->  struct foo;
        """
    )
    parser.add_argument('input', nargs='?', default='-',
                       help='Input file (default: stdin)')
    parser.add_argument('-o', '--output',
                       help='Output file (default: stdout)')
    parser.add_argument('--keep-usage', action='store_true',
                       help='Keep TYPENAME usage (don\'t convert to struct typename)')
    
    args = parser.parse_args()
    
    # Read input
    if args.input == '-':
        content = sys.stdin.read()
    else:
        with open(args.input, 'r') as f:
            content = f.read()
    
    # Extract typedef map BEFORE normalization
    typedef_map = extract_typedef_map(content)
    
    # Apply normalizations (order matters!)
    if not args.keep_usage:
        content = normalize_typedef_usage(content, typedef_map)
    
    content = normalize_typedef_struct(content)
    content = normalize_forward_typedef(content)
    
    # Write output
    if args.output:
        with open(args.output, 'w') as f:
            f.write(content)
    else:
        print(content, end='')


if __name__ == '__main__':
    main()
