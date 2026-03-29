#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class ExceptionAnalyzer {
public:
  ExceptionAnalyzer() = default;

  bool functionThrows(const clang::FunctionDecl *Func) {
    if (!Func)
      return false;

    auto CacheIt = ThrowCache.find(Func);
    if (CacheIt != ThrowCache.end()) {
      return CacheIt->second;
    }

    if (const auto *FPT = Func->getType()->getAs<clang::FunctionProtoType>()) {
      if (FPT->getExceptionSpecType() != clang::EST_None) {
        bool throws =
            (FPT->getExceptionSpecType() != clang::EST_BasicNoexcept &&
             FPT->getExceptionSpecType() != clang::EST_NoexceptTrue);
        ThrowCache[Func] = throws;
        return throws;
      }
    }

    if (!Func->hasBody()) {
      return true;
    }

    if (!InProgress.insert(Func).second) {
      return false;
    }

    StmtAnalyzer Analyzer(*this);
    Analyzer.TraverseStmt(Func->getBody());

    InProgress.erase(Func);

    ThrowCache[Func] = Analyzer.hasThrow();
    return ThrowCache[Func];
  }

private:
  llvm::DenseMap<const clang::FunctionDecl *, bool> ThrowCache;
  llvm::SmallPtrSet<const clang::FunctionDecl *, 8> InProgress;

  class StmtAnalyzer : public clang::RecursiveASTVisitor<StmtAnalyzer> {
  public:
    explicit StmtAnalyzer(ExceptionAnalyzer &EA) : ParentAnalyzer(EA) {}

    bool hasThrow() const { return Throws; }

    bool VisitCXXThrowExpr(clang::CXXThrowExpr *E) {
      Throws = true;
      return false;
    }

    bool VisitCXXNewExpr(clang::CXXNewExpr *E) {
      Throws = true;
      return false;
    }

    bool VisitCallExpr(clang::CallExpr *Call) {
      if (Throws)
        return false;

      if (const clang::FunctionDecl *Callee = Call->getDirectCallee()) {
        if (ParentAnalyzer.functionThrows(Callee)) {
          Throws = true;
          return false;
        }
      }
      return true;
    }

    bool VisitCXXConstructExpr(clang::CXXConstructExpr *CtorExpr) {
      if (Throws)
        return false;

      if (const clang::CXXConstructorDecl *Ctor = CtorExpr->getConstructor()) {
        if (ParentAnalyzer.functionThrows(Ctor)) {
          Throws = true;
          return false;
        }
      }
      return true;
    }

  private:
    ExceptionAnalyzer &ParentAnalyzer;
    bool Throws = false;
  };
};

class NikitinaVVisitor final
    : public clang::RecursiveASTVisitor<NikitinaVVisitor> {
public:
  explicit NikitinaVVisitor(clang::ASTContext *Ctx) : Ctx(Ctx) {}

  bool VisitFunctionDecl(clang::FunctionDecl *Func) {
    if (!Func->hasBody() || Func->isMain()) {
      return true;
    }

    const auto *FPT = Func->getType()->getAs<clang::FunctionProtoType>();
    if (!FPT || FPT->getExceptionSpecType() != clang::EST_None) {
      return true;
    }

    if (!Analyzer.functionThrows(Func)) {
      clang::FunctionProtoType::ExtProtoInfo EPI = FPT->getExtProtoInfo();
      EPI.ExceptionSpec.Type = clang::EST_BasicNoexcept;

      clang::QualType UpdatedType =
          Ctx->getFunctionType(FPT->getReturnType(), FPT->getParamTypes(), EPI);

      Func->setType(UpdatedType);
    }

    return true;
  }

private:
  clang::ASTContext *Ctx;
  ExceptionAnalyzer Analyzer;
};

class NikitinaVConsumer final : public clang::ASTConsumer {
public:
  explicit NikitinaVConsumer(clang::ASTContext *Ctx) : Visitor(Ctx) {}

  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
  }

private:
  NikitinaVVisitor Visitor;
};

class NikitinaVAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<NikitinaVConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<NikitinaVAction>
    X("nikitina_v_noexcept_plugin",
      "Automatically adds noexcept to safe functions");