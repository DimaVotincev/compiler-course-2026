#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

class OverrideVisitor final
    : public clang::RecursiveASTVisitor<OverrideVisitor> {
public:
  bool VisitCXXMethodDecl(clang::CXXMethodDecl *method) {
    if (method->isImplicit())
      return true;

    if (!method->isVirtual())
      return true;

    if (method->size_overridden_methods() == 0)
      return true;

    if (method->hasAttr<clang::OverrideAttr>())
      return true;

    auto &DE = method->getASTContext().getDiagnostics();
    unsigned diagID = DE.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "method '%0' overrides base method but is not marked 'override'");
    DE.Report(method->getLocation(), diagID) << method->getName();

    return true;
  }
};

class OverrideConsumer final : public clang::ASTConsumer {
public:
  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  OverrideVisitor m_visitor;
};

class OverrideAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &, llvm::StringRef) override {
    return std::make_unique<OverrideConsumer>();
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<OverrideAction>
    X("override_check", "Find overriding methods without override specifier");