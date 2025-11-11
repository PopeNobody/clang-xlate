// clang-bake.cpp
// ---------------------------------------------------------------
// A tiny Clang-Tooling utility that
//   • bakes command-line -D values into #if / #ifdef expressions
//   • ignores #define inside the file (optional)
//   • can query “what symbols must be defined to fully bake this header?”
// ---------------------------------------------------------------

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Lexer.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include "clang/Rewrite/Core/Rewriter.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <map>
#include <set>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory BakeCategory("clang-bake options");

static cl::opt<bool> IgnoreDefines(
    "ignore-defines",
    cl::desc("Ignore #define statements that appear inside the file"),
    cl::cat(BakeCategory));

static cl::opt<bool> QueryMode(
    "query",
    cl::desc("Print the list of symbols that must be defined to bake the header"),
    cl::cat(BakeCategory));

static cl::opt<std::string> OutputFile(
    "o",
    cl::desc("Write the baked header to this file (default: stdout)"),
    cl::cat(BakeCategory));

// ------------------------------------------------------------------
// 1. Query visitor – walks the AST and records identifiers that are
//    used inside #if conditions but are not known to the preprocessor.
// ------------------------------------------------------------------
class QueryVisitor : public RecursiveASTVisitor<QueryVisitor> {
    std::set<std::string> Undefined;
    const Preprocessor &PP;

public:
    explicit QueryVisitor(const Preprocessor &PP) : PP(PP) {}

    bool VisitIdentifierExpr(IdentifierExpr *E) {
        IdentifierInfo *II = E->getIdentifierInfo();
        if (!II) return true;
        if (!PP.getIdentifierInfo(II->getName())) {
            Undefined.insert(II->getName().str());
        }
        return true;
    }

    void print() const {
        if (Undefined.empty()) {
            outs() << "All symbols are already defined – header is fully baked.\n";
            return;
        }
        outs() << "To fully bake this header, define the following symbols:\n";
        for (const auto &sym : Undefined)
            outs() << "  -D" << sym << "\n";
    }
};

// ------------------------------------------------------------------
// 2. PPCallbacks that rewrites #if condition text with known macro
//    values (e.g. #if FOO>BAR → #if 5>BAR)
// ------------------------------------------------------------------
class BakePPCallbacks : public PPCallbacks {
    Rewriter &Rewrite;
    const std::map<std::string, std::string> &KnownDefs;

public:
    BakePPCallbacks(Rewriter &R,
                    const std::map<std::string, std::string> &Defs)
        : Rewrite(R), KnownDefs(Defs) {}

    void If(SourceLocation Loc,
            SourceRange ConditionRange,
            ConditionResult Result) override {

        if (IgnoreDefines) return;   // user asked to keep everything

        std::string cond = Lexer::getSourceText(
            CharSourceRange::getTokenRange(ConditionRange),
            Rewrite.getSourceMgr(),
            Rewrite.getLangOpts());

        std::string baked = cond;
        for (const auto &[name, value] : KnownDefs) {
            size_t pos = 0;
            while ((pos = baked.find(name, pos)) != std::string::npos) {
                baked.replace(pos, name.length(), value);
                pos += value.length();
            }
        }

        // Replace the original condition text
        Rewrite.ReplaceText(ConditionRange, baked);
    }
};

// ------------------------------------------------------------------
// 3. Frontend action – runs the preprocessor, installs callbacks,
//    optionally runs the query visitor, and finally writes the result.
// ------------------------------------------------------------------
class BakeAction : public PreprocessOnlyAction {
protected:
    void ExecuteAction() override {
        CompilerInstance &CI = getCompilerInstance();
        Rewriter Rewrite(CI.getSourceManager(), CI.getLangOpts());

        // --------------------------------------------------------------
        // Collect every -Dfoo=bar that the user passed on the command line.
        // --------------------------------------------------------------
        std::map<std::string, std::string> KnownDefs;
        for (const auto &D : CI.getPreprocessor().getPredefines()) {
            size_t eq = D.find('=');
            if (eq != std::string::npos) {
                std::string name = D.substr(0, eq);
                std::string value = D.substr(eq + 1);
                KnownDefs[name] = value;
            }
        }

        // Install the baking callbacks
        CI.getPreprocessor().addPPCallbacks(
            std::make_unique<BakePPCallbacks>(Rewrite, KnownDefs));

        // Run the normal preprocessing (this populates the Rewriter)
        PreprocessOnlyAction::ExecuteAction();

        // --------------------------------------------------------------
        // Query mode – run the AST visitor to find undefined identifiers.
        // --------------------------------------------------------------
        if (QueryMode) {
            QueryVisitor QV(CI.getPreprocessor());
            // Translate the preprocessed buffer into an AST
            ASTContext &Ctx = CI.getASTContext();
            TranslationUnitDecl *TU = Ctx.getTranslationUnitDecl();
            QV.TraverseDecl(TU);
            QV.print();
            return;
        }

        // --------------------------------------------------------------
        // Normal bake mode – write the rewritten source.
        // --------------------------------------------------------------
        std::error_code EC;
        raw_fd_ostream OS(OutputFile.empty() ? "-" : OutputFile, EC);
        if (EC) {
            errs() << "Error opening output file: " << EC.message() << "\n";
            return;
        }

        const RewriteBuffer &Buf =
            Rewrite.getEditBuffer(Rewrite.getSourceMgr().getMainFileID());
        Buf.write(OS);
    }
};

// ------------------------------------------------------------------
// 4. main() – glue everything together with Clang’s Tooling API.
// ------------------------------------------------------------------
int main(int argc, const char **argv) {
    auto ExpectedParser =
        CommonOptionsParser::create(argc, argv, BakeCategory);
    if (!ExpectedParser) {
        errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());

    return Tool.run(newFrontendActionFactory<BakeAction>().get());
}