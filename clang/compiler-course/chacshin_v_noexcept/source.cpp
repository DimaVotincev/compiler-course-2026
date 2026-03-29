#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

namespace {

class ChacshinNoexceptAnalyzer final
    : public clang::RecursiveASTVisitor<ChacshinNoexceptAnalyzer> {
public:
  bool DetectedThrow = false;
  clang::ASTContext *Context;
  std::unordered_set<const clang::FunctionDecl *> VisitedFunctions;

  ChacshinNoexceptAnalyzer(clang::ASTContext *Ctx) : Context(Ctx) {}

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *) {
    DetectedThrow = true;
    return true;
  }
  bool VisitCXXNewExpr(clang::CXXNewExpr *) {
    DetectedThrow = true;
    return true;
  }
  bool VisitCXXDynamicCastExpr(clang::CXXDynamicCastExpr *DynCast) {
    clang::QualType T = DynCast->getTypeAsWritten();
    if (T->isReferenceType()) {
      DetectedThrow = true;
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *Call) {
    if (DetectedThrow)
      return true;

    if (auto *Callee = Call->getDirectCallee()) {
      if (canFunctionThrow(Callee))
        DetectedThrow = true;
    } else {
      clang::QualType CalleeType = Call->getCallee()->getType();
      if (auto *PT = CalleeType->getAs<clang::PointerType>()) {
        if (auto *FPT =
                PT->getPointeeType()->getAs<clang::FunctionProtoType>()) {
          if (FPT->getExceptionSpecType() != clang::EST_BasicNoexcept &&
              FPT->getExceptionSpecType() != clang::EST_NoexceptTrue) {
            DetectedThrow = true;
          }
        }
      }
    }
    return true;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *Ctor) {
    if (DetectedThrow)
      return true;
    if (auto *CtorDecl = Ctor->getConstructor()) {
      if (canFunctionThrow(CtorDecl))
        DetectedThrow = true;
    }
    return true;
  }

private:
  bool canFunctionThrow(const clang::FunctionDecl *FD) {
    if (!FD || VisitedFunctions.count(FD))
      return false;
    VisitedFunctions.insert(FD);

    auto *Proto = FD->getType()->getAs<clang::FunctionProtoType>();
    if (!Proto)
      return false;

    if (Proto->getExceptionSpecType() == clang::EST_None) {
      if (!FD->hasBody())
        return true;
      ChacshinNoexceptAnalyzer SubAnalyzer(Context);
      SubAnalyzer.VisitedFunctions = VisitedFunctions;
      SubAnalyzer.TraverseStmt(FD->getBody());
      VisitedFunctions = SubAnalyzer.VisitedFunctions;
      return SubAnalyzer.DetectedThrow;
    }
    return false;
  }
};

class ChacshinNoexceptVisitor final
    : public clang::RecursiveASTVisitor<ChacshinNoexceptVisitor> {
public:
  explicit ChacshinNoexceptVisitor(clang::ASTContext *Ctx) : Context(Ctx) {}

  bool VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (!FD->hasBody())
      return true;

    auto *Proto = FD->getType()->getAs<clang::FunctionProtoType>();
    if (!Proto || Proto->getExceptionSpecType() != clang::EST_None)
      return true;
    if (FD->isVirtualAsWritten())
      return true;

    ChacshinNoexceptAnalyzer Analyzer(Context);
    Analyzer.TraverseStmt(FD->getBody());

    if (!Analyzer.DetectedThrow) {
      clang::FunctionProtoType::ExtProtoInfo EPI = Proto->getExtProtoInfo();
      EPI.ExceptionSpec.Type = clang::EST_BasicNoexcept;
      clang::QualType NewType = Context->getFunctionType(
          Proto->getReturnType(), Proto->getParamTypes(), EPI);
      FD->setType(NewType);

      llvm::errs() << "Function " << FD->getNameAsString()
                   << " marked noexcept\n";
    } else {
      llvm::errs() << "Function " << FD->getNameAsString()
                   << " remains potentially throwing\n";
    }
    return true;
  }

private:
  clang::ASTContext *Context;
};

class ChacshinNoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit ChacshinNoexceptConsumer(clang::ASTContext *Ctx) : Visitor(Ctx) {}

  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    llvm::errs() << "Plugin is running!\n";
    for (auto *D : Ctx.getTranslationUnitDecl()->decls())
      Visitor.TraverseDecl(D);
  }

private:
  ChacshinNoexceptVisitor Visitor;
};

class ChacshinNoexceptAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<ChacshinNoexceptConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<ChacshinNoexceptAction>
    X("chacshin_noexcept_plugin",
      "Automatically adds noexcept to functions that do not throw");