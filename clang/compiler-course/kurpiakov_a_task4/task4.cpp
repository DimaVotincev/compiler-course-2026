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
      : m_context(context), global_var_counter(0), static_var_counter(0),
        local_var_counter(0), func_parm_counter(0) {}

  bool VisitVarDecl(clang::VarDecl *var) {

    if (var != var->getCanonicalDecl()) {
      return true;
    }

    if (var->isFileVarDecl()) {
      if (var->getStorageClass() == clang::SC_Static) {
        static_var_counter += 1;
      } else {
        global_var_counter += 1;
      }
    } else if (var->isStaticLocal()) {
      static_var_counter += 1;
    } else if (var->isLocalVarDeclOrParm()) {
      if (var->isLocalVarDecl()) {
        local_var_counter += 1;
      } else {
        func_parm_counter += 1;
      }
    }

    return true;
  }

  void print() const {
    llvm::outs() << "Total count : "
                 << global_var_counter + static_var_counter +
                        local_var_counter + func_parm_counter
                 << "\n";
    llvm::outs() << "Global variables : " << global_var_counter << "\n";
    llvm::outs() << "Static variables : " << static_var_counter << "\n";
    llvm::outs() << "Local variables  : " << local_var_counter << "\n";
    llvm::outs() << "Function params  : " << func_parm_counter << "\n";
  }

private:
  clang::ASTContext *m_context;
  size_t global_var_counter;
  size_t static_var_counter;
  size_t local_var_counter;
  size_t func_parm_counter;
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
    X("var_statistic", "Description plugin");
