#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/Basic/ExceptionSpecificationType.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseSet.h"

namespace {
class GutyanskyAstThrowFinder final
    : public clang::RecursiveASTVisitor<GutyanskyAstThrowFinder> {
public:
  explicit GutyanskyAstThrowFinder(clang::ASTContext *context)
      : m_context(context), m_hasThrow(false) {}

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *expr) {
    m_hasThrow = true;
    return true;
  }

  bool VisitCXXNewExpr(clang::CXXNewExpr *expr) {
    m_hasThrow = true;
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *expr) {
    if (!m_hasThrow) {
      const clang::FunctionDecl *decl = expr->getDirectCallee();
      if (decl == nullptr || !IsNoThrow(decl)) {
        m_hasThrow = true;
      }
    }

    return true;
  }

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *expr) {
    if (!m_hasThrow) {
      const clang::FunctionDecl *decl = expr->getDirectCallee();
      if (decl == nullptr || !IsNoThrow(decl)) {
        m_hasThrow = true;
      }
    }

    return true;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *expr) {
    if (!m_hasThrow) {
      const clang::CXXConstructorDecl *decl = expr->getConstructor();
      if (decl == nullptr || !IsNoThrow(decl)) {
        m_hasThrow = true;
      }
    }

    return true;
  }

  bool HasThrow() const { return m_hasThrow; }

private:
  clang::ASTContext *m_context;
  llvm::DenseSet<const clang::FunctionDecl *> m_checked;
  bool m_hasThrow;

  bool IsNoThrow(const clang::FunctionDecl *decl) {
    if (!decl)
      return false;

    if (m_checked.contains(decl)) {
      return true;
    }
    m_checked.insert(decl);

    if (decl->isVirtualAsWritten()) {
      return false;
    }

    const auto *proto = decl->getType()->getAs<clang::FunctionProtoType>();
    if (!proto) {
      return false;
    }

    if (proto->hasNoexceptExceptionSpec()) {
      return proto->isNothrow();
    }

    if (decl->getExceptionSpecType() != clang::EST_None) {
      return false;
    }

    if (decl->hasBody()) {
      GutyanskyAstThrowFinder finder(m_context);
      finder.m_checked = m_checked;
      finder.TraverseStmt(decl->getBody());
      m_checked = finder.m_checked;

      return !finder.HasThrow();
    }

    return false;
  }
};

class GutyanskyAAstNoexceptVisitor final
    : public clang::RecursiveASTVisitor<GutyanskyAAstNoexceptVisitor> {
public:
  explicit GutyanskyAAstNoexceptVisitor(clang::ASTContext *context)
      : m_context(context) {}
  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (NeedNoexcept(func)) {
      const auto *proto = func->getType()->getAs<clang::FunctionProtoType>();
      clang::FunctionProtoType::ExtProtoInfo info = proto->getExtProtoInfo();
      info.ExceptionSpec.Type = clang::EST_BasicNoexcept;

      clang::QualType type = m_context->getFunctionTypeWithExceptionSpec(
          func->getType(), info.ExceptionSpec);
      func->setType(type);
    }

    func->dump();

    return true;
  }

private:
  clang::ASTContext *m_context;

  bool NeedNoexcept(const clang::FunctionDecl *func) const {
    if (!func->hasBody()) {
      return false;
    }

    if (func->getExceptionSpecType() !=
        clang::ExceptionSpecificationType::EST_None) {
      return false;
    }

    if (func->isVirtualAsWritten()) {
      return false;
    }

    GutyanskyAstThrowFinder finder(m_context);
    finder.TraverseStmt(func->getBody());

    return !finder.HasThrow();
  }
};

class GutyanskyAAstNoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit GutyanskyAAstNoexceptConsumer(clang::ASTContext *context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  GutyanskyAAstNoexceptVisitor m_visitor;
};

class GutyanskyAAstNoexceptAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<GutyanskyAAstNoexceptConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<GutyanskyAAstNoexceptAction>
    X("gutyansky_a_ast_noexcept_plugin",
      "Adds noexcept to those functions that do not throw exceptions");
