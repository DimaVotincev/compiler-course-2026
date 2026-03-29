#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

class OverrideVisitor : public clang::RecursiveASTVisitor<OverrideVisitor> {
public:
  bool VisitCXXMethodDecl(clang::CXXMethodDecl *MD) {

    if (MD->isImplicit())
      return true;

    if (!MD->isVirtual())
      return true;

    if (MD->size_overridden_methods() == 0)
      return true;

    if (MD->hasAttr<clang::OverrideAttr>())
      return true;

    auto &Diag = MD->getASTContext().getDiagnostics();
    unsigned WarnID =
        Diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                             "virtual method is not marked 'override'");
    Diag.Report(MD->getLocation(), WarnID) << MD;

    return true;
  }
};

class OverrideConsumer : public clang::ASTConsumer {
public:
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    m_visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OverrideVisitor m_visitor;
};

class OverrideAction : public clang::PluginASTAction {
protected:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<OverrideConsumer>();
  }

  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<OverrideAction>
    X("OverrideCheckPlugin",
      "warn if overriding virtual method is not marked override");
