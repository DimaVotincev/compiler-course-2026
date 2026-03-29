#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>

namespace {
class ChyokotovAVisitor final
    : public clang::RecursiveASTVisitor<ChyokotovAVisitor> {
public:
  explicit ChyokotovAVisitor(clang::ASTContext *context) : m_context(context) {}

  bool VisitBinaryOperator(clang::BinaryOperator *binop) {
    if (!binop || !binop->isAssignmentOp()) {
      return true;
    }
    clang::Expr *rhs = binop->getRHS()->IgnoreImplicit();
    if (isAllocation(rhs)) {
      clang::VarDecl *var = getVarDecl(binop->getLHS()->IgnoreImplicit());
      if (var) {
        vars[var] = 3;
        Loc[var] = binop->getBeginLoc();
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *func = call->getDirectCallee();
    if (!func) {
      return true;
    }

    llvm::StringRef funcName = func->getName();
    if (funcName == "free" || funcName == "fclose") {
      if (call->getNumArgs() > 0) {
        clang::Expr *arg = call->getArg(0)->IgnoreParenCasts();
        clang::VarDecl *var = getVarDecl(arg);
        if (var) {
          vars[var] = 0;
        }
      }
    }
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *var) {
    clang::Expr *exp = var->getInit();
    if (exp && isAllocation(exp)) {
      vars[var] = 1;
    }
    return true;
  }

  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    clang::SourceManager &sm = m_context->getSourceManager();
    clang::SourceLocation retLoc = ret->getBeginLoc();

    clang::Expr *retValue = ret->getRetValue();
    if (retValue) {
      retValue = retValue->IgnoreParenCasts();
      if (auto *declRef = clang::dyn_cast<clang::DeclRefExpr>(retValue)) {
        if (auto *var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
          if (vars[var] == 1 || vars[var] == 3) {
            vars[var] = 2;
            Loc[var] = ret->getReturnLoc();

            clang::DiagnosticsEngine &DE = m_context->getDiagnostics();
            unsigned returnLeakDiagID =
                DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                   "resource leak: '%0' may not be freed (no "
                                   "guaranteed deallocation on return)");
            DE.Report(ret->getReturnLoc(), returnLeakDiagID)
                << var->getNameAsString();
          }
        }
      }
    }

    for (auto &[var, state] : vars) {
      if ((state == 1 || state == 3) && state != 2) {
        clang::FunctionDecl *varFunc = nullptr;
        clang::DeclContext *dc = var->getDeclContext();
        while (dc) {
          if (clang::FunctionDecl *fd =
                  clang::dyn_cast<clang::FunctionDecl>(dc)) {
            varFunc = fd;
            break;
          }
          dc = dc->getParent();
        }

        if (!varFunc)
          continue;

        clang::SourceLocation funcStart = varFunc->getBeginLoc();
        clang::SourceLocation funcEnd = varFunc->getEndLoc();

        if (sm.isBeforeInTranslationUnit(funcStart, retLoc) &&
            sm.isBeforeInTranslationUnit(retLoc, funcEnd)) {

          clang::DiagnosticsEngine &DE = m_context->getDiagnostics();
          unsigned returnLeakDiagID =
              DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                 "resource leak: '%0' may not be freed (no "
                                 "guaranteed deallocation on return)");
          DE.Report(ret->getReturnLoc(), returnLeakDiagID)
              << var->getNameAsString();
        }
      }
    }

    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *exp) {
    clang::Expr *arg = exp->getArgument()->IgnoreParenCasts();
    clang::VarDecl *var = getVarDecl(arg);
    if (var) {
      vars[var] = 0;
    }
    return true;
  }

  void outputs() {
    if (vars.empty()) {
      return;
    }
    clang::DiagnosticsEngine &DE = m_context->getDiagnostics();

    unsigned leakDiagID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                             "memory leak: '%0'");

    for (auto &[var, state] : vars) {
      if (state == 1) {
        DE.Report(var->getLocation(), leakDiagID) << var->getNameAsString();
      } else if (state == 3) {
        DE.Report(Loc[var], leakDiagID) << var->getNameAsString();
      }
    }
  }

private:
  bool isAllocation(clang::Expr *exp) {
    clang::Expr *castexp = exp->IgnoreParenCasts();

    if (clang::CallExpr *call = clang::dyn_cast<clang::CallExpr>(castexp)) {
      if (clang::FunctionDecl *func = call->getDirectCallee()) {
        llvm::StringRef funcName = func->getName();
        return (funcName == "malloc" || funcName == "fopen");
      }
    }
    return clang::isa<clang::CXXNewExpr>(castexp);
  }

  clang::VarDecl *getVarDecl(clang::Expr *exp) {
    if (!exp) {
      return nullptr;
    }
    clang::Expr *castexp = exp->IgnoreParenCasts();
    if (clang::DeclRefExpr *ref =
            clang::dyn_cast<clang::DeclRefExpr>(castexp)) {
      return clang::dyn_cast<clang::VarDecl>(ref->getDecl());
    }
    return nullptr;
  }

  clang::ASTContext *m_context;
  std::map<clang::VarDecl *, int> vars;
  std::map<clang::VarDecl *, clang::SourceLocation> Loc;
};

class ExampleConsumer final : public clang::ASTConsumer {
public:
  explicit ExampleConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.outputs();
  }

private:
  ChyokotovAVisitor m_visitor;
};

class ExampleAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<ExampleConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

  ActionType getActionType() override { return AddAfterMainAction; }
};
} // namespace

static clang::FrontendPluginRegistry::Add<ExampleAction>
    X("chyokotov_a_analyzer_plugin", "Description_plugin");
