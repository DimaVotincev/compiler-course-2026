#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;

namespace {

class OvsyannikovOverrideVisitor
    : public RecursiveASTVisitor<OvsyannikovOverrideVisitor> {
public:
  explicit OvsyannikovOverrideVisitor(ASTContext *Context) : Context(Context) {}

  bool VisitCXXMethodDecl(CXXMethodDecl *Method) {
    if (Method->size_overridden_methods() > 0 &&
        !Method->hasAttr<OverrideAttr>() && !Method->isImplicit()) {

      DiagnosticsEngine &DiagEngine = Context->getDiagnostics();
      unsigned DiagID = DiagEngine.getCustomDiagID(
          DiagnosticsEngine::Warning,
          "ovsyannikov-check: method '%0' overrides a virtual function but "
          "lacks 'override' specifier");

      DiagEngine.Report(Method->getLocation(), DiagID)
          << Method->getNameAsString();
    }
    return true;
  }

private:
  ASTContext *Context;
};

class OvsyannikovOverrideConsumer : public ASTConsumer {
public:
  explicit OvsyannikovOverrideConsumer(ASTContext *Context)
      : Visitor(Context) {}
  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OvsyannikovOverrideVisitor Visitor;
};

class OvsyannikovOverrideAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef) override {
    return std::make_unique<OvsyannikovOverrideConsumer>(&CI.getASTContext());
  }
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<OvsyannikovOverrideAction>
    X("ovsyannikov_no_override", "Warns about missing 'override' specifiers");
