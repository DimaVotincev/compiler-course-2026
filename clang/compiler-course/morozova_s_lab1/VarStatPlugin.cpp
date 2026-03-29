#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

class VarStatVisitor : public RecursiveASTVisitor<VarStatVisitor> {
public:
  explicit VarStatVisitor(ASTContext *Context)
      : globalCount(0), localCount(0), staticLocalCount(0), paramCount(0) {}

  bool VisitVarDecl(VarDecl *VD) {
    if (!VD->isThisDeclarationADefinition())
      return true;

    if (VD->isFileVarDecl()) {
      globalCount++;
    } else if (VD->isLocalVarDecl() && !VD->isStaticLocal()) {
      localCount++;
    } else if (VD->isStaticLocal()) {
      staticLocalCount++;
    }
    return true;
  }

  bool VisitParmVarDecl(ParmVarDecl *PD) {
    if (auto *FD = dyn_cast<FunctionDecl>(PD->getDeclContext()))
      if (!FD->isTemplated() || isa<CXXConstructorDecl>(FD))
        paramCount++;
    return true;
  }

  void print() {
    llvm::outs() << "========== СТАТИСТИКА ПЕРЕМЕННЫХ ==========\n";
    llvm::outs() << "Глобальных переменных: " << globalCount << "\n";
    llvm::outs() << "Локальных переменных: " << localCount << "\n";
    llvm::outs() << "Статических локальных: " << staticLocalCount << "\n";
    llvm::outs() << "Параметров функций: " << paramCount << "\n";
    llvm::outs() << "===========================================\n";
  }

private:
  int globalCount;
  int localCount;
  int staticLocalCount;
  int paramCount;
};

class VarStatConsumer : public ASTConsumer {
public:
  explicit VarStatConsumer(ASTContext *Context) : Visitor(Context) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.print();
  }

private:
  VarStatVisitor Visitor;
};

class VarStatAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef InFile) override {
    return std::make_unique<VarStatConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }

  ActionType getActionType() override { return PluginASTAction::ReplaceAction; }
};

static FrontendPluginRegistry::Add<VarStatAction>
    X("var-stat", "variable statistics plugin");
