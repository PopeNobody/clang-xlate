// clang-bake.cpp — FINAL, CLANG 19 COMPATIBLE
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Lexer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory BakeCategory("clang-bake options");
static cl::opt<bool> IgnoreDefines("ignore-defines", cl::desc("Ignore #define in file"), cl::cat(BakeCategory));
static cl::opt<bool> QueryMode("query", cl::desc("List required symbols"), cl::cat(BakeCategory));
static cl::opt<std::string> OutputFile("o", cl::desc("Output file"), cl::cat(BakeCategory));

class QueryVisitor : public RecursiveASTVisitor<QueryVisitor> {
    std::set<std::string> Undefined;
    const Preprocessor &PP;
public:
    explicit QueryVisitor(const Preprocessor &PP) : PP(PP) {}
    bool VisitIdentifierExpr(DeclRefExpr *E) {
        if (auto *II = E->getIdentifierInfo()) {
            if (!PP.getMacroDefinition(II)) {
                Undefined.insert(II->getName().str());
            }
        }
        return true;
    }
    void print() const {
        if (Undefined.empty()) { outs() << "Fully baked.\n"; return; }
        outs() << "Define:\n";
        for (const auto &s : Undefined) outs() << "  -D" << s << "\n";
    }
};

class BakePPCallbacks : public PPCallbacks {
    Rewriter &R;
    const std::map<std::string, std::string> &Defs;
public:
    BakePPCallbacks(Rewriter &R, const std::map<std::string, std::string> &D)
        : R(R), Defs(D) {}
    void If(SourceLocation, SourceRange CondRange, ConditionResult) override {
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
protected:
    void ExecuteAction() override {
        CompilerInstance &CI = getCompilerInstance();
        Rewriter R(CI.getSourceManager(), CI.getLangOpts());

        // FIXED: Parse getPredefines() as string
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

        CI.getPreprocessor().addPPCallbacks(
            std::make_unique<BakePPCallbacks>(R, Defs));

        PreprocessOnlyAction::ExecuteAction();

        if (QueryMode) {
            QueryVisitor QV(CI.getPreprocessor());
            if (auto *TU = CI.getASTContext().getTranslationUnitDecl())
                QV.TraverseDecl(TU);
            QV.print();
            return;
        }

        std::error_code EC;
        raw_fd_ostream OS(OutputFile.empty() ? "-" : OutputFile, EC);
        if (EC) { errs() << "Write error: " << EC.message() << "\n"; return; }
        R.getEditBuffer(R.getSourceMgr().getMainFileID()).write(OS);
    }
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, BakeCategory);
    if (!ExpectedParser) { errs() << ExpectedParser.takeError(); return 1; }
    ClangTool Tool(ExpectedParser->getCompilations(), ExpectedParser->getSourcePathList());
    return Tool.run(newFrontendActionFactory<BakeAction>().get());
}
