#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;

namespace {

class OverrideWarningVisitor
    : public RecursiveASTVisitor<OverrideWarningVisitor> {
public:
  OverrideWarningVisitor(DiagnosticsEngine &Diags, unsigned DiagID)
      : Diags(Diags), OverrideMissingDiagID(DiagID) {}

  bool VisitCXXMethodDecl(CXXMethodDecl *Method) {
    if (Method->isImplicit())
      return true;

    if (!Method->isVirtual())
      return true;

    if (Method->size_overridden_methods() == 0)
      return true;

    if (Method->hasAttr<OverrideAttr>())
      return true;

    Diags.Report(Method->getLocation(), OverrideMissingDiagID)
        << Method->getSourceRange();
    return true;
  }

private:
  DiagnosticsEngine &Diags;
  unsigned OverrideMissingDiagID;
};

class OverrideWarningConsumer final : public ASTConsumer {
public:
  OverrideWarningConsumer(DiagnosticsEngine &Diags, unsigned DiagID)
      : Visitor(Diags, DiagID) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OverrideWarningVisitor Visitor;
};

class OverrideWarningAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    DiagnosticsEngine &Diags = CI.getDiagnostics();
    unsigned DiagID = Diags.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "overriding virtual function without 'override' specifier");
    return std::make_unique<OverrideWarningConsumer>(Diags, DiagID);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<OverrideWarningAction>
    X("override_warning", "Warns about virtual functions that override but "
                          "miss the 'override' specifier");