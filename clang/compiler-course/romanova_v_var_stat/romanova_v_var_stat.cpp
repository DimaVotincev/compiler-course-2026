#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class VarStatVisitor final : public clang::RecursiveASTVisitor<VarStatVisitor> {
public:
  explicit VarStatVisitor(clang::ASTContext *context)
      : m_context(context), static_var_counter(0), local_var_counter(0),
        global_var_counter(0), func_param_counter(0) {}
  bool VisitVarDecl(clang::VarDecl *variable) {

    if (!variable->isThisDeclarationADefinition() &&
        variable->getStorageClass() ==
            clang::SC_Extern) { // для extern и прочих объявлений
      return true;
    }

    if (variable->getStorageClass() == clang::SC_Static) {
      static_var_counter++;
    } else if (variable->isLocalVarDeclOrParm()) {
      if (variable->isLocalVarDecl())
        local_var_counter++;
      else
        func_param_counter++;
    } else if (variable->isFileVarDecl())
      global_var_counter++;

    return true;
  }

  void print() const {
    llvm::outs() << "static variables: " << static_var_counter << "\n";
    llvm::outs() << "local variables: " << local_var_counter << "\n";
    llvm::outs() << "global variables: " << global_var_counter << "\n";
    llvm::outs() << "function parameters: " << func_param_counter << "\n";
    llvm::outs() << "total: "
                 << static_var_counter + local_var_counter +
                        global_var_counter + func_param_counter
                 << "\n";
  }

private:
  clang::ASTContext *m_context;
  size_t static_var_counter{};
  size_t local_var_counter{};
  size_t global_var_counter{};
  size_t func_param_counter{};
};

class VarStatConsumer final : public clang::ASTConsumer {
public:
  explicit VarStatConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.print();
  }

private:
  VarStatVisitor m_visitor;
};

class VarStatAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<VarStatConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<VarStatAction>
    X("romanova_v_var_stat_plugin", "Description plugin");
