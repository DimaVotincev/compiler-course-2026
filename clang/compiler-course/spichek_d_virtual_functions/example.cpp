#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

class MissingOverrideVisitor final
    : public clang::RecursiveASTVisitor<MissingOverrideVisitor> {
public:
  MissingOverrideVisitor(clang::ASTContext *context,
                         clang::CompilerInstance &ci)
      : m_context(context), m_ci(ci) {
    m_diagID = m_ci.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "virtual function overrides a base class virtual function but is not "
        "marked with 'override'");
  }

  bool VisitCXXMethodDecl(clang::CXXMethodDecl *MD) {
    if (MD->size_overridden_methods() > 0) {
      if (!MD->hasAttr<clang::OverrideAttr>()) {
        m_ci.getDiagnostics().Report(MD->getLocation(), m_diagID);
      }
    }
    return true;
  }

private:
  clang::ASTContext *m_context;
  clang::CompilerInstance &m_ci;
  unsigned m_diagID;
};

class MissingOverrideConsumer final : public clang::ASTConsumer {
public:
  MissingOverrideConsumer(clang::ASTContext *context,
                          clang::CompilerInstance &ci)
      : m_visitor(context, ci) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  MissingOverrideVisitor m_visitor;
};

class MissingOverrideAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<MissingOverrideConsumer>(&ci.getASTContext(), ci);
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<MissingOverrideAction>
    X("missing_override_plugin", "Warns about missing override specifiers");