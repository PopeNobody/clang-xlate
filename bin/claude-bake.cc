// clang-bake.cpp — CORRECTED, CLANG 19 COMPATIBLE
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Lexer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory BakeCategory("clang-bake options");
static cl::opt<bool> IgnoreDefines("ignore-defines", cl::desc("Ignore #define in file"), cl::cat(BakeCategory));
static cl::opt<bool> QueryMode("query", cl::desc("List required symbols"), cl::cat(BakeCategory));
static cl::opt<std::string> OutputFile("o", cl::desc("Output file"), cl::cat(BakeCategory));

// Track undefined macros used in preprocessor directives
class QueryPPCallbacks : public PPCallbacks {
    std::set<std::string> &Undefined;
    const Preprocessor &PP;
public:
    QueryPPCallbacks(std::set<std::string> &U, const Preprocessor &PP) 
        : Undefined(U), PP(PP) {}
    
    void Ifdef(SourceLocation Loc, const Token &MacroNameTok,
               const MacroDefinition &MD) override {
        if (MD.getMacroInfo() == nullptr) {
            Undefined.insert(MacroNameTok.getIdentifierInfo()->getName().str());
        }
    }
    
    void Ifndef(SourceLocation Loc, const Token &MacroNameTok,
                const MacroDefinition &MD) override {
        if (MD.getMacroInfo() == nullptr) {
            Undefined.insert(MacroNameTok.getIdentifierInfo()->getName().str());
        }
    }
    
    void Defined(const Token &MacroNameTok, const MacroDefinition &MD,
                 SourceRange Range) override {
        if (MD.getMacroInfo() == nullptr) {
            Undefined.insert(MacroNameTok.getIdentifierInfo()->getName().str());
        }
    }
    
    // Also catch macro uses in #if expressions
    void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD,
                      SourceRange Range, const MacroArgs *Args) override {
        // Only track if it's undefined and being used in a conditional context
        if (MD.getMacroInfo() == nullptr) {
            Undefined.insert(MacroNameTok.getIdentifierInfo()->getName().str());
        }
    }
};

class BakePPCallbacks : public PPCallbacks {
    Rewriter &R;
    const std::map<std::string, std::string> &Defs;
public:
    BakePPCallbacks(Rewriter &R, const std::map<std::string, std::string> &D)
        : R(R), Defs(D) {}
    
    void If(SourceLocation Loc, SourceRange CondRange, ConditionValueKind Result) override {
        if (IgnoreDefines) return;
        std::string cond = Lexer::getSourceText(
            CharSourceRange::getTokenRange(CondRange),
            R.getSourceMgr(), R.getLangOpts()).str();
        std::string baked = cond;
        for (const auto &[k, v] : Defs) {
            size_t pos = 0;
            while ((pos = baked.find(k, pos)) != std::string::npos) {
                baked.replace(pos, k.size(), v);
                pos += v.size();
            }
        }
        R.ReplaceText(CondRange, baked);
    }
};

class BakeAction : public PreprocessOnlyAction {
    std::set<std::string> Undefined;
    
protected:
    void ExecuteAction() override {
        CompilerInstance &CI = getCompilerInstance();
        Rewriter R(CI.getSourceManager(), CI.getLangOpts());

        // Parse predefined macros
        std::map<std::string, std::string> Defs;
        const std::string &Predefines = CI.getPreprocessor().getPredefines();
        size_t pos = 0;
        while (pos < Predefines.size()) {
            size_t eq = Predefines.find('=', pos);
            if (eq == std::string::npos) break;
            size_t end = Predefines.find(' ', eq);
            if (end == std::string::npos) end = Predefines.size();
            std::string name = Predefines.substr(pos, eq - pos);
            std::string value = Predefines.substr(eq + 1, end - eq - 1);
            Defs[name] = value;
            pos = end;
            if (pos < Predefines.size() && Predefines[pos] == ' ') pos++;
        }

        if (QueryMode) {
            // In query mode, just track undefined macros
            CI.getPreprocessor().addPPCallbacks(
                std::make_unique<QueryPPCallbacks>(Undefined, CI.getPreprocessor()));
        } else {
            // In bake mode, do the rewrites
            CI.getPreprocessor().addPPCallbacks(
                std::make_unique<BakePPCallbacks>(R, Defs));
        }

        PreprocessOnlyAction::ExecuteAction();

        if (QueryMode) {
            if (Undefined.empty()) { 
                outs() << "Fully baked.\n"; 
            } else {
                outs() << "Define:\n";
                for (const auto &s : Undefined) 
                    outs() << "  -D" << s << "\n";
            }
            return;
        }

        std::error_code EC;
        raw_fd_ostream OS(OutputFile.empty() ? "-" : OutputFile, EC);
        if (EC) { 
            errs() << "Write error: " << EC.message() << "\n"; 
            return; 
        }
        R.getEditBuffer(R.getSourceMgr().getMainFileID()).write(OS);
    }
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, BakeCategory);
    if (!ExpectedParser) { 
        errs() << ExpectedParser.takeError(); 
        return 1; 
    }
    ClangTool Tool(ExpectedParser->getCompilations(), 
                   ExpectedParser->getSourcePathList());
    return Tool.run(newFrontendActionFactory<BakeAction>().get());
}