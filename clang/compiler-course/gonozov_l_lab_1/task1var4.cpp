#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class VariablesStatisticsVisitor final
    : public clang::RecursiveASTVisitor<VariablesStatisticsVisitor> {
public:
  explicit VariablesStatisticsVisitor(clang::ASTContext *context)
      : global_count(0), local_count(0), static_count(0), param_count(0) {}

  bool shouldVisitTemplateInstantiations() const { return false; }

  bool VisitVarDecl(clang::VarDecl *var) {
    if (llvm::isa<clang::ParmVarDecl>(var))
      return true;
    if (var != var->getCanonicalDecl())
      return true;

    if (var->isStaticLocal()) {
      static_count++;
    } else if (var->isFileVarDecl() &&
               var->getStorageClass() == clang::SC_Static) {
      if (var->isThisDeclarationADefinition())
        static_count++;
    } else if (var->isFileVarDecl()) {
      if (var->isThisDeclarationADefinition())
        global_count++;
    } else if (var->isLocalVarDecl()) {
      local_count++;
    }

    return true;
  }

  bool VisitParmVarDecl(clang::ParmVarDecl *param) {
    auto *f = llvm::dyn_cast<clang::FunctionDecl>(param->getDeclContext());
    if (!f || !f->isThisDeclarationADefinition())
      return true;
    param_count++;
    return true;
  }

  void PrintStatistics() {
    llvm::errs() << "Total count: "
                 << global_count + local_count + static_count + param_count
                 << "\n";
    llvm::errs() << "Global variables: " << global_count << "\n";
    llvm::errs() << "Local variables: " << local_count << "\n";
    llvm::errs() << "Static variables: " << static_count << "\n";
    llvm::errs() << "Function parameters: " << param_count << "\n";
  }

private:
  size_t global_count;
  size_t local_count;
  size_t static_count;
  size_t param_count;
};

class VariablesStatisticsConsumer final : public clang::ASTConsumer {
public:
  explicit VariablesStatisticsConsumer(clang::ASTContext *context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.PrintStatistics();
  }

private:
  VariablesStatisticsVisitor m_visitor;
};

class VariablesStatisticsAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<VariablesStatisticsConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<VariablesStatisticsAction>
    X("variables_statistics_plugin",
      "Plugin that collects statistics on various types of variables");
