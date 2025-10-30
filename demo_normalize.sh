#!/bin/bash
# Demo of typedef normalization + declaration extraction workflow

echo "=== Typedef Normalization + Declaration Extraction Demo ==="
echo ""

echo "1. Original file (test_typedef.c):"
echo "   Contains typedef struct patterns common in bash"
head -20 test_typedef.c
echo "   ..."
echo ""

echo "2. Normalizing typedefs..."
python3 normalize_typedef.py test_typedef.c -o /tmp/normalized.c
echo "   Generated /tmp/normalized.c"
echo ""

echo "3. Normalized version:"
head -20 /tmp/normalized.c
echo "   ..."
echo ""

echo "4. Extracting declarations from normalized file:"
./decl_extractor_cpp /tmp/normalized.c 2>/dev/null
echo ""

echo "5. Now with quickfix format for vim:"
./decl_extractor_cpp /tmp/normalized.c 2>/dev/null > /tmp/normalized.qf
echo "   Generated /tmp/normalized.qf with $(wc -l < /tmp/normalized.qf) declarations"
echo ""

echo "6. Finding all struct word_list usage:"
grep 'word_list' /tmp/normalized.qf
echo ""

echo "=== Complete Workflow for Bash Files ==="
echo ""
echo "# Normalize typedef patterns"
echo "python3 normalize_typedef.py bashfile.c -o /tmp/normalized.c"
echo ""
echo "# Extract declarations"
echo "./decl_extractor /tmp/normalized.c -u -- -std=gnu89 -I./include > /tmp/decls.qf"
echo ""
echo "# Open in vim"
echo "vim -q /tmp/decls.qf"
echo ""
echo "# Or do it all in one pipeline:"
echo "python3 normalize_typedef.py bashfile.c | gcc -E - -I./include | ..."
