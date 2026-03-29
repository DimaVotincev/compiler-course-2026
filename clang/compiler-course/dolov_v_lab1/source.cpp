#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

class MissingOverrideVisitor final
    : public clang::RecursiveASTVisitor<MissingOverrideVisitor> {
public:
  explicit MissingOverrideVisitor(clang::ASTContext *context)
      : m_context(context) {}

  bool VisitCXXMethodDecl(clang::CXXMethodDecl *method_decl) {
    if (method_decl->isImplicit())
      return true;
    if (!method_decl->getParent())
      return true;

    if (m_context->getSourceManager().isInSystemHeader(
            method_decl->getLocation()))
      return true;

    if (method_decl->size_overridden_methods() == 0)
      return true;

    if (method_decl->hasAttr<clang::OverrideAttr>())
      return true;

    clang::DiagnosticsEngine &diag_engine = m_context->getDiagnostics();

    unsigned main_diag_id =
        diag_engine.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                    "method '%0' overrides a virtual function "
                                    "but is missing the 'override' specifier");

    unsigned note_diag_id = diag_engine.getCustomDiagID(
        clang::DiagnosticsEngine::Note, "overridden virtual function is here");

    diag_engine.Report(method_decl->getLocation(), main_diag_id)
        << method_decl->getNameAsString();

    for (auto *overridden_method : method_decl->overridden_methods()) {
      diag_engine.Report(overridden_method->getLocation(), note_diag_id);
    }

    return true;
  }

private:
  clang::ASTContext *m_context;
};

class MissingOverrideConsumer final : public clang::ASTConsumer {
public:
  explicit MissingOverrideConsumer(clang::ASTContext *context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  MissingOverrideVisitor m_visitor;
};

class MissingOverrideAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef) override {
    return std::make_unique<MissingOverrideConsumer>(&compiler.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<MissingOverrideAction>
    X("dolov_v_lab1",
      "A professional plugin to enforce 'override' specifiers usage");