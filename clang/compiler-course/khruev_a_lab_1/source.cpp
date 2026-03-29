#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class VariablesStatisticVisitor final
    : public clang::RecursiveASTVisitor<VariablesStatisticVisitor> {
private:
  clang::ASTContext *m_context;
  int m_global_count;
  int m_static_count;
  int m_local_count;
  int m_param_count;

public:
  explicit VariablesStatisticVisitor(clang::ASTContext *context)
      : m_context(context), m_global_count(0), m_static_count(0),
        m_local_count(0), m_param_count(0) {}

  bool shouldVisitTemplateInstantiations() const { return false; }

  bool VisitVarDecl(clang::VarDecl *var) {
    if (!var->isFirstDecl()) {
      return true;
    }

    if (llvm::isa<clang::ParmVarDecl>(var)) {
      m_param_count++;
    } else if (var->getStorageClass() == clang::SC_Static ||
               var->isStaticLocal()) {
      m_static_count++;
    } else if (var->isLocalVarDecl()) {
      m_local_count++;
    } else if (var->hasGlobalStorage()) {
      m_global_count++;
    }

    return true;
  }

  void PrintStatistics() const {
    int total = m_global_count + m_static_count + m_local_count + m_param_count;
    llvm::errs() << "Total count : " << total << "\n"
                 << "Global variables : " << m_global_count << "\n"
                 << "Static variables : " << m_static_count << "\n"
                 << "Local variables  : " << m_local_count << "\n"
                 << "Function params  : " << m_param_count << "\n";
  }
};

class VariablesStatisticConsumer final : public clang::ASTConsumer {
public:
  explicit VariablesStatisticConsumer(clang::ASTContext *context)
      : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.PrintStatistics();
  }

private:
  VariablesStatisticVisitor m_visitor;
};

class VariablesStatisticAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<VariablesStatisticConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<VariablesStatisticAction>
    X("variables_statistic",
      "Plugin for counting variables by their scope and type");