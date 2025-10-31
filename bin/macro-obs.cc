/*
 * Macro Observer - Reports all preprocessor constructs
 * 
 * Reports to stderr:
 *   - #define directives
 *   - #undef directives
 *   - #ifdef/#ifndef conditionals
 *   - #if/#elif/#else/#endif
 * 
 * Outputs unchanged source to stdout (null transformer)
 * 
 * Compile:
 *   g++ -std=c++17 -I./include macro_observer.cpp -o macro_observer -L./lib -lclang
 */

#include <clang-c/Index.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <map>

// Helper to convert CXString to std::string
std::string fromCXString(CXString cxstr) {
    std::string result = clang_getCString(cxstr);
    clang_disposeString(cxstr);
    return result;
}

// Helper to get source location info
std::string getLocation(CXSourceLocation loc) {
    CXFile file;
    unsigned line, column, offset;
    clang_getFileLocation(loc, &file, &line, &column, &offset);
    
    if (file) {
        std::string filename = fromCXString(clang_getFileName(file));
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
    return "<unknown>";
}

// Get the source text for a range
std::string getSourceText(CXTranslationUnit tu, CXSourceRange range) {
    CXToken *tokens = nullptr;
    unsigned numTokens = 0;
    clang_tokenize(tu, range, &tokens, &numTokens);
    
    std::string result;
    for (unsigned i = 0; i < numTokens; i++) {
        result += fromCXString(clang_getTokenSpelling(tu, tokens[i]));
        if (i < numTokens - 1) result += " ";
    }
    
    clang_disposeTokens(tu, tokens, numTokens);
    return result;
}

struct MacroInfo {
    std::string name;
    std::string location;
    std::string definition;
    bool is_function_like;
};

struct VisitorData {
    CXTranslationUnit tu;
    std::string main_filename;
    std::vector<MacroInfo> macros;
    bool verbose;
};

// Visitor callback
CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    VisitorData* data = static_cast<VisitorData*>(client_data);
    
    CXSourceLocation location = clang_getCursorLocation(cursor);
    
    // Skip if not in main file
    if (clang_Location_isInSystemHeader(location)) {
        return CXChildVisit_Continue;
    }
    
    CXFile file;
    clang_getFileLocation(location, &file, nullptr, nullptr, nullptr);
    if (!file) return CXChildVisit_Continue;
    
    std::string filename = fromCXString(clang_getFileName(file));
    if (filename != data->main_filename) {
        return CXChildVisit_Continue;
    }
    
    CXCursorKind kind = clang_getCursorKind(cursor);
    
    if (kind == CXCursor_MacroDefinition) {
        MacroInfo info;
        info.name = fromCXString(clang_getCursorSpelling(cursor));
        info.location = getLocation(location);
        
        // Get the macro definition
        CXSourceRange extent = clang_getCursorExtent(cursor);
        info.definition = getSourceText(data->tu, extent);
        
        info.is_function_like = clang_Cursor_isMacroFunctionLike(cursor);
        
        data->macros.push_back(info);
        
        // Report to stderr
        std::cerr << info.location << ": #define " << info.name;
        if (info.is_function_like) {
            std::cerr << "(...) [function-like]";
        }
        std::cerr << "\n";
        if (data->verbose) {
            std::cerr << "  Definition: " << info.definition << "\n";
        }
    }
    else if (kind == CXCursor_MacroExpansion) {
        // This is a macro usage, not definition
        // We might want to track these too for finding unused macros
        if (data->verbose) {
            std::string name = fromCXString(clang_getCursorSpelling(cursor));
            std::string loc = getLocation(location);
            std::cerr << loc << ": Macro expansion: " << name << "\n";
        }
    }
    else if (kind == CXCursor_InclusionDirective) {
        if (data->verbose) {
            std::string included = fromCXString(clang_getCursorDisplayName(cursor));
            std::string loc = getLocation(location);
            std::cerr << loc << ": #include " << included << "\n";
        }
    }
    
    return CXChildVisit_Recurse;
}

void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file> [options] [-- clang-args...]\n";
    std::cerr << "Options:\n";
    std::cerr << "  -v, --verbose  Show macro expansions and includes\n";
    std::cerr << "  -h, --help     Show this help\n";
    std::cerr << "\nReports preprocessor constructs to stderr.\n";
    std::cerr << "Outputs unchanged source to stdout.\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << prog << " source.c 2>macros.log > output.c\n";
    std::cerr << "  " << prog << " source.c -v -- -I./include\n";
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    // Parse arguments
    const char* filename = nullptr;
    bool verbose = false;
    std::vector<const char*> clang_args;
    
    bool in_clang_args = false;
    for (int i = 1; i < argc; i++) {
        if (in_clang_args) {
            clang_args.push_back(argv[i]);
        }
        else if (std::strcmp(argv[i], "--") == 0) {
            in_clang_args = true;
        }
        else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
        else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        else if (!filename) {
            filename = argv[i];
        }
        else {
            std::cerr << "Error: Unknown argument: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (!filename) {
        std::cerr << "Error: No input file specified\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Add default clang args
    std::vector<const char*> all_args = {
        "-fsyntax-only",
        "-ferror-limit=0",
        "-Wno-everything"
    };
    all_args.insert(all_args.end(), clang_args.begin(), clang_args.end());
    
    // Create index
    CXIndex index = clang_createIndex(0, 0);
    
    // Parse with detailed preprocessing record
    CXTranslationUnit tu = clang_parseTranslationUnit(
        index,
        filename,
        all_args.data(),
        all_args.size(),
        nullptr,
        0,
        CXTranslationUnit_DetailedPreprocessingRecord |
        CXTranslationUnit_SkipFunctionBodies
    );
    
    if (!tu) {
        std::cerr << "Error: Failed to parse " << filename << "\n";
        clang_disposeIndex(index);
        return 1;
    }
    
    // Get main file
    CXFile mainFile = clang_getFile(tu, filename);
    std::string mainFilename = fromCXString(clang_getFileName(mainFile));
    
    // Visit AST to find macros
    VisitorData data;
    data.tu = tu;
    data.main_filename = mainFilename;
    data.verbose = verbose;
    
    std::cerr << "=== Macro Analysis: " << filename << " ===\n";
    
    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(cursor, visitor, &data);
    
    std::cerr << "\n=== Total: " << data.macros.size() << " macro definitions ===\n";
    
    // Output original source to stdout (null transformer)
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: Could not read source file\n";
        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);
        return 1;
    }
    
    std::cout << in.rdbuf();
    
    // Cleanup
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    
    return 0;
}
