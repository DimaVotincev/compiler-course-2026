#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class FuncChecker final : public clang::RecursiveASTVisitor<FuncChecker> {
public:
  bool Flag = false;
  clang::ASTContext *Context;
  llvm::SmallPtrSet<clang::FunctionDecl *, 16> Visited;

  FuncChecker(clang::ASTContext *Ctx) : Context(Ctx) {}
  bool VisitCXXThrowExpr(clang::CXXThrowExpr *E) {
    Flag = true;
    return false;
  }

  bool VisitCXXNewExpr(clang::CXXNewExpr *E) {
    Flag = true;
    return false;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (Flag)
      return false;
    if (clang::FunctionDecl *caller = call->getDirectCallee()) {
      if (functionCanThrow(caller)) {
        Flag = true;
        return false;
      }
    }

    return true;
  }
  bool VisitCXXConstructExpr(clang::CXXConstructExpr *ctor) {
    if (Flag)
      return false;

    if (clang::CXXConstructorDecl *constructor = ctor->getConstructor()) {
      if (functionCanThrow(constructor)) {
        Flag = true;
        return false;
      }
    }
    return true;
  }

private:
  bool functionCanThrow(clang::FunctionDecl *func) {
    if (!func)
      return false;

    if (Visited.count(func))
      return false;
    Visited.insert(func);

    const auto *ftp = func->getType()->getAs<clang::FunctionProtoType>();
    if (ftp) {
      if (ftp->getExceptionSpecType() == clang::EST_None) {
        if (!func->hasBody())
          return true;
        FuncChecker check(Context);
        check.Visited = Visited;
        check.TraverseStmt(func->getBody());
        Visited = check.Visited;
        return check.Flag;
      }
    }

    return false;
  }
};
class PapulinaYVisitor final
    : public clang::RecursiveASTVisitor<PapulinaYVisitor> {
public:
  explicit PapulinaYVisitor(clang::ASTContext *context) : m_context(context) {}
  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (!func->hasBody()) {
      return true;
    }
    const auto *ftp = func->getType()->getAs<clang::FunctionProtoType>();
    if (!ftp) {
      return true;
    }
    if (ftp->getExceptionSpecType() != clang::EST_None) {
      return true;
    }
    FuncChecker check(m_context);
    check.TraverseStmt(func->getBody());
    if (!check.Flag) {
      clang::FunctionProtoType::ExtProtoInfo epi = ftp->getExtProtoInfo();
      epi.ExceptionSpec.Type = clang::EST_BasicNoexcept;

      clang::QualType newType = m_context->getFunctionType(
          ftp->getReturnType(), ftp->getParamTypes(), epi);
      func->setType(newType);
    }
    const auto *newFtp = func->getType()->getAs<clang::FunctionProtoType>();
    llvm::outs() << "Function " << func->getNameAsString()
                 << ": exception spec: " << newFtp->getExceptionSpecType()
                 << "\n";
    return true;
  }

private:
  clang::ASTContext *m_context;
};

class PapulinaYConsumer final : public clang::ASTConsumer {
public:
  explicit PapulinaYConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  PapulinaYVisitor m_visitor;
};

class PapulinaYAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<PapulinaYConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
  ActionType getActionType() override { return AddBeforeMainAction; }
};
} // namespace

static clang::FrontendPluginRegistry::Add<PapulinaYAction>
    X("papulina_y_noexcept_func_plugin",
      "Adds the noexcept specifier to all functions that do not throw "
      "exceptions");
