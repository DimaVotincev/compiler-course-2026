#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct Counter {
  int globals = 0;
  int statics = 0;
  int locals = 0;
  int params = 0;
};

class VarVisitorKruglova
    : public clang::RecursiveASTVisitor<VarVisitorKruglova> {
public:
  Counter results;

  bool VisitParmVarDecl(clang::ParmVarDecl *P) {
    results.params++;
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *V) {
    if (llvm::isa<clang::ParmVarDecl>(V))
      return true;

    if (V->isStaticLocal() || V->getStorageClass() == clang::SC_Static ||
        V->isStaticDataMember()) {
      results.statics++;
    } else if (V->isLocalVarDecl()) {
      results.locals++;
    } else if (V->hasGlobalStorage()) {
      results.globals++;
    }

    return true;
  }
};

class VarConsumerKruglova : public clang::ASTConsumer {
public:
  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    VarVisitorKruglova V;
    V.TraverseDecl(Ctx.getTranslationUnitDecl());

    int total = V.results.globals + V.results.statics + V.results.locals +
                V.results.params;

    llvm::outs() << "Statistics\n"
                 << "Globals variables: " << V.results.globals << "\n"
                 << "Statics variables: " << V.results.statics << "\n"
                 << "Locals variables:  " << V.results.locals << "\n"
                 << "Params variables:  " << V.results.params << "\n"
                 << "Total variables:   " << total << "\n";
  }
};

class VarActionKruglova : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<VarConsumerKruglova>();
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<VarActionKruglova>
    X("kruglova_plugin", "Var stats");
