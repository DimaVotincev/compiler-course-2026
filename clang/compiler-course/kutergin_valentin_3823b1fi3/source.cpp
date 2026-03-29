#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class ThrowDetector final : public clang::RecursiveASTVisitor<ThrowDetector> {
public:
    bool canThrow = false;

    bool VisitCXXThrowExpr(clang::CXXThrowExpr *E) {
        canThrow = true;
        return false;
    }

    bool VisitCallExpr(clang::CallExpr *E) {
        if (clang::FunctionDecl *Callee = E->getDirectCallee()) {
            const auto *Proto = Callee->getType()->getAs<clang::FunctionProtoType>();
            if (!Proto || Proto->getExceptionSpecType() == clang::EST_None) {
                canThrow = true;
                return false;
            }
        }
        return true;
    }
};

class NoexceptVisitor final : public clang::RecursiveASTVisitor<NoexceptVisitor> {
public:
    explicit NoexceptVisitor(clang::ASTContext &C) : Context(C) {}
    
    bool VisitFunctionDecl(clang::FunctionDecl *FD) {
        if (!FD->hasBody())
            return true;

        const auto *FPT = FD->getType()->getAs<clang::FunctionProtoType>();
        if (!FPT || FPT->getExceptionSpecType() != clang::EST_None)
            return true;

        ThrowDetector Detector;
        Detector.TraverseStmt(FD->getBody());

        if (!Detector.canThrow) {
            clang::FunctionProtoType::ExtProtoInfo EPI = FPT->getExtProtoInfo();
            EPI.ExceptionSpec.Type = clang::EST_BasicNoexcept;

            clang::QualType NewType = Context.getFunctionType(FPT->getReturnType(), FPT->getParamTypes(), EPI);

            FD->setType(NewType);
        }
        return true;
    }
private:
    clang::ASTContext &Context;
};

class NoexceptConsumer final : public clang::ASTConsumer {
public:
    void HandleTranslationUnit(clang::ASTContext &Context) override {
        NoexceptVisitor Visitor(Context);
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }
};

class NoexceptAction final : public clang::PluginASTAction {
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
        return std::make_unique<NoexceptConsumer>();
    }

    bool ParseArgs(const clang::CompilerInstance &ci, const std::vector<std::string> &args) override {
        return true;
    }

    ActionType getActionType() override {
        return AddBeforeMainAction;
    }
};
} // namespace

static clang::FrontendPluginRegistry::Add<NoexceptAction> 
    X("kutergin_valentin_3823b1fi3", "NoexceptSpecificatorPlugin");