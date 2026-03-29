#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class ThrowScanner : public clang::RecursiveASTVisitor<ThrowScanner> {
public:
  bool found = false;

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *E) {
    found = true;
    return false;
  }
  bool VisitCXXNewExpr(clang::CXXNewExpr *E) {
    found = true;
    return false;
  }
  bool VisitCallExpr(clang::CallExpr *E) {
    if (clang::FunctionDecl *Callee = E->getDirectCallee()) {

      const auto *ftp = Callee->getType()->getAs<clang::FunctionProtoType>();
      if (ftp && ftp->getExceptionSpecType() != clang::EST_BasicNoexcept) {
        found = true;
        return false;
      }
    }
    return true;
  }
};

class SmyshlaevAVisitor final
    : public clang::RecursiveASTVisitor<SmyshlaevAVisitor> {
public:
  explicit SmyshlaevAVisitor(clang::ASTContext *context) : m_context(context) {}
  llvm::SmallPtrSet<const clang::FunctionDecl *, 16> DangerousFunctions;
  bool VisitFunctionDecl(clang::FunctionDecl *func) {

    if (!func->hasBody())
      return true;

    const auto *ftp = func->getType()->getAs<clang::FunctionProtoType>();
    if (!ftp)
      return true;
    if (ftp->getExceptionSpecType() != clang::EST_None)
      return true;

    ThrowScanner scanner;
    scanner.TraverseStmt(func->getBody());
    if (!scanner.found) {
      clang::FunctionProtoType::ExtProtoInfo epi = ftp->getExtProtoInfo();
      epi.ExceptionSpec.Type = clang::EST_BasicNoexcept;

      clang::QualType newType = m_context->getFunctionType(
          ftp->getReturnType(), ftp->getParamTypes(), epi);
      func->setType(newType);
      llvm::outs() << "Функция " << func->getNameAsString()
                   << " теперь noexcept!\n";
    } else {
      llvm::outs() << "Функция " << func->getNameAsString()
                   << " осталась неизмененной\n";
    }

    return true;
  }

private:
  clang::ASTContext *m_context;
};

class SmyshlaevAConsumer final : public clang::ASTConsumer {
public:
  explicit SmyshlaevAConsumer(clang::ASTContext *context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  SmyshlaevAVisitor m_visitor;
};

class SmyshlaevAAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<SmyshlaevAConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

  clang::PluginASTAction::ActionType getActionType() override {
    return AddBeforeMainAction;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<SmyshlaevAAction>
    X("smyshlaev_a_lab1_plugin", "Description plugin");
