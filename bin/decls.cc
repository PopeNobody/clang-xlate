/*
 * Extract and display all declarations/definitions from C/C++ code using libtooling.
 * 
 * Compile:
 *   clang++ -std=c++17 decl_extractor_tool.cpp -o decl_extractor \
 *     $(llvm-config --cxxflags --ldflags --system-libs --libs support) \
 *     -lclangTooling -lclangFrontend -lclangDriver -lclangSerialization \
 *     -lclangParse -lclangSema -lclangAnalysis -lclangEdit \
 *     -lclangAST -lclangLex -lclangBasic
 *
 * Or with pkg-config (if available):
 *   clang++ -std=c++17 decl_extractor_tool.cpp -o decl_extractor \
 *     $(pkg-config --cflags --libs clang)
 */

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "llvm/Support/CommandLine.h"
#include <iostream>
#include <vector>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// Command line options
static cl::OptionCategory ToolCategory("decl-extractor options");
static cl::opt<bool> ShowMacros("m", cl::desc("Include macro definitions"), 
                                 cl::cat(ToolCategory));
static cl::opt<bool> DefinitionsOnly("d", cl::desc("Show only definitions"), 
                                      cl::cat(ToolCategory));

struct DeclInfo {
    std::string kind;
    std::string declaration;
    bool is_definition;
    unsigned line;
    unsigned column;
};

// Visitor that walks the AST and collects declarations
class DeclVisitor : public RecursiveASTVisitor<DeclVisitor> {
public:
    explicit DeclVisitor(ASTContext *Context, SourceManager &SM, 
                         std::vector<DeclInfo> &Decls)
        : Context(Context), SM(SM), Decls(Decls) {}

    bool shouldVisitDecl(Decl *D) {
        if (!D->getLocation().isValid())
            return false;
        
        // Only process declarations in the main file
        if (!SM.isInMainFile(D->getLocation()))
            return false;
        
        return true;
    }

    std::string getFunctionSignature(FunctionDecl *FD) {
        std::string result;
        QualType RT = FD->getReturnType();
        result = RT.getAsString() + " " + FD->getNameAsString() + "(";
        
        unsigned NumParams = FD->getNumParams();
        if (NumParams == 0) {
            result += "void";
        } else {
            for (unsigned i = 0; i < NumParams; ++i) {
                if (i > 0) result += ", ";
                ParmVarDecl *Param = FD->getParamDecl(i);
                result += Param->getType().getAsString();
                if (!Param->getNameAsString().empty()) {
                    result += " " + Param->getNameAsString();
                }
            }
        }
        result += ")";
        return result;
    }

    std::string getDeclarationString(Decl *D) {
        std::string result;
        
        if (auto *FD = dyn_cast<FunctionDecl>(D)) {
            result = getFunctionSignature(FD) + ";";
        } 
        else if (auto *VD = dyn_cast<VarDecl>(D)) {
            result = VD->getType().getAsString() + " " + VD->getNameAsString() + ";";
        }
        else if (auto *TD = dyn_cast<TypedefDecl>(D)) {
            QualType UnderlyingType = TD->getUnderlyingType();
            result = "typedef " + UnderlyingType.getAsString() + " " + 
                     TD->getNameAsString() + ";";
        }
        else if (auto *RD = dyn_cast<RecordDecl>(D)) {
            std::string name = RD->getNameAsString();
            if (name.empty()) name = "<anonymous>";
            result = std::string(RD->isStruct() ? "struct " : 
                                RD->isUnion() ? "union " : "class ") + name + ";";
        }
        else if (auto *ED = dyn_cast<EnumDecl>(D)) {
            std::string name = ED->getNameAsString();
            if (name.empty()) name = "<anonymous>";
            result = "enum " + name + ";";
        }
        else if (auto *FD = dyn_cast<FieldDecl>(D)) {
            result = FD->getType().getAsString() + " " + FD->getNameAsString() + ";";
        }
        else if (auto *ECD = dyn_cast<EnumConstantDecl>(D)) {
            result = ECD->getNameAsString();
            if (ECD->getInitExpr()) {
                // Could print the value here if we evaluate the expression
                result += " = <value>";
            }
        }
        else {
            result = D->getDeclKindName() + std::string(": ") + 
                     cast<NamedDecl>(D)->getNameAsString();
        }
        
        return result;
    }

    bool isDefinition(Decl *D) {
        if (auto *FD = dyn_cast<FunctionDecl>(D)) {
            return FD->isThisDeclarationADefinition();
        }
        else if (auto *RD = dyn_cast<RecordDecl>(D)) {
            return RD->isThisDeclarationADefinition();
        }
        else if (auto *ED = dyn_cast<EnumDecl>(D)) {
            return ED->isThisDeclarationADefinition();
        }
        return false;
    }

    void addDeclaration(Decl *D) {
        if (!shouldVisitDecl(D))
            return;

        DeclInfo info;
        info.kind = D->getDeclKindName();
        info.declaration = getDeclarationString(D);
        info.is_definition = isDefinition(D);
        
        SourceLocation Loc = D->getLocation();
        info.line = SM.getSpellingLineNumber(Loc);
        info.column = SM.getSpellingColumnNumber(Loc);
        
        Decls.push_back(info);
    }

    bool VisitFunctionDecl(FunctionDecl *FD) {
        addDeclaration(FD);
        return true;
    }

    bool VisitVarDecl(VarDecl *VD) {
        addDeclaration(VD);
        return true;
    }

    bool VisitTypedefDecl(TypedefDecl *TD) {
        addDeclaration(TD);
        return true;
    }

    bool VisitRecordDecl(RecordDecl *RD) {
        addDeclaration(RD);
        return true;
    }

    bool VisitEnumDecl(EnumDecl *ED) {
        addDeclaration(ED);
        return true;
    }

    bool VisitFieldDecl(FieldDecl *FD) {
        addDeclaration(FD);
        return true;
    }

    bool VisitEnumConstantDecl(EnumConstantDecl *ECD) {
        addDeclaration(ECD);
        return true;
    }

private:
    ASTContext *Context;
    SourceManager &SM;
    std::vector<DeclInfo> &Decls;
};

// AST Consumer that creates the visitor
class DeclConsumer : public ASTConsumer {
public:
    explicit DeclConsumer(ASTContext *Context, std::vector<DeclInfo> &Decls)
        : Visitor(Context, Context->getSourceManager(), Decls) {}

    virtual void HandleTranslationUnit(ASTContext &Context) {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    DeclVisitor Visitor;
};

// Frontend Action that creates the consumer
class DeclAction : public ASTFrontendAction {
public:
    explicit DeclAction(std::vector<DeclInfo> &Decls) : Decls(Decls) {}

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance &CI, StringRef file) {
        return std::make_unique<DeclConsumer>(&CI.getASTContext(), Decls);
    }

private:
    std::vector<DeclInfo> &Decls;
};

// Factory for creating our action
class DeclActionFactory : public FrontendActionFactory {
public:
    explicit DeclActionFactory(std::vector<DeclInfo> &Decls) : Decls(Decls) {}

    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<DeclAction>(Decls);
    }

private:
    std::vector<DeclInfo> &Decls;
};

int main(int argc, const char **argv) {
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
    if (!ExpectedParser) {
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());

    std::vector<DeclInfo> Decls;
    DeclActionFactory Factory(Decls);
    
    int Result = Tool.run(&Factory);
    
    // Print results
    if (OptionsParser.getSourcePathList().size() > 0) {
        std::cout << "=== Declarations from " 
                  << OptionsParser.getSourcePathList()[0] << " ===\n\n";
    }
    
    for (const auto &decl : Decls) {
        if (DefinitionsOnly && !decl.is_definition)
            continue;
        
        std::cout << decl.declaration << "  // "
                  << (decl.is_definition ? "definition" : "declaration")
                  << " at " << decl.line << ":" << decl.column << "\n";
    }
    
    std::cout << "\n=== Total: " << Decls.size() << " items ===\n";
    
    return Result;
}
