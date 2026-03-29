#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class ThrowFinder final : public clang::RecursiveASTVisitor<ThrowFinder> {
public:
  bool VisitCXXThrowExpr(clang::CXXThrowExpr *) {
    m_hasThrow = true;

    return false;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (auto *callee = call->getDirectCallee()) {
      const auto *proto = callee->getType()->getAs<clang::FunctionProtoType>();
      if (!proto || proto->getExceptionSpecType() == clang::EST_None) {
        m_hasThrow = true;

        return false;
      }
    }

    return true;
  }

  bool hasThrow() const { return m_hasThrow; }

private:
  bool m_hasThrow = false;
};

class SpecNoexceptVisitor final
    : public clang::RecursiveASTVisitor<SpecNoexceptVisitor> {
public:
  explicit SpecNoexceptVisitor(clang::ASTContext &context)
      : m_context(context) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (!func->hasBody())
      return true;

    const auto *proto = func->getType()->getAs<clang::FunctionProtoType>();
    if (!proto || proto->getExceptionSpecType() != clang::EST_None)
      return true;

    ThrowFinder finder;
    finder.TraverseStmt(func->getBody());

    if (!finder.hasThrow()) {
      clang::FunctionProtoType::ExtProtoInfo epi = proto->getExtProtoInfo();
      epi.ExceptionSpec.Type = clang::EST_BasicNoexcept;
      clang::QualType newType = m_context.getFunctionType(
          proto->getReturnType(), proto->getParamTypes(), epi);
      func->setType(newType);
      func->dump();
    }

    return true;
  }

private:
  clang::ASTContext &m_context;
};

class SpecNoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit SpecNoexceptConsumer(clang::ASTContext &context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  SpecNoexceptVisitor m_visitor;
};

class SpecNoexceptAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<SpecNoexceptConsumer>(ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<SpecNoexceptAction>
    X("spec_noexcept", "Add noexcept specifier to functions that don't throw");
