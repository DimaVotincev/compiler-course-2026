#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>

namespace {
enum class ResourceState { Freed, Allocated, Returned };

struct ResourceInfo {
  std::string type;
  ResourceState state;
  clang::SourceLocation loc;
};

class NikolaevDVisitor final
    : public clang::RecursiveASTVisitor<NikolaevDVisitor> {
public:
  explicit NikolaevDVisitor(clang::ASTContext *context) : m_context(context) {}

  bool VisitBinaryOperator(clang::BinaryOperator *binop) {
    if (!binop || !binop->isAssignmentOp())
      return true;

    clang::Expr *rhs = binop->getRHS()->IgnoreImplicit();

    std::string type = getAllocationType(rhs);
    if (type.empty())
      return true;

    clang::VarDecl *var = getVarDecl(binop->getLHS()->IgnoreImplicit());

    if (var) {
      vars[var] = {type, ResourceState::Allocated, binop->getBeginLoc()};
    }

    return true;
  }

  bool VisitVarDecl(clang::VarDecl *var) {
    clang::Expr *exp = var->getInit();
    if (!exp)
      return true;

    std::string type = getAllocationType(exp);

    if (!type.empty()) {
      vars[var] = {type, ResourceState::Allocated, var->getLocation()};
    }

    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *func = call->getDirectCallee();
    if (!func)
      return true;

    llvm::StringRef funcName = func->getName();

    if (funcName == "free" || funcName == "fclose") {
      if (call->getNumArgs() > 0) {
        clang::Expr *arg = call->getArg(0)->IgnoreParenCasts();
        clang::VarDecl *var = getVarDecl(arg);
        if (var && vars.count(var)) {
          vars[var].state = ResourceState::Freed;
        }
      }
    }

    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *exp) {
    clang::Expr *arg = exp->getArgument()->IgnoreParenCasts();
    clang::VarDecl *var = getVarDecl(arg);

    if (var && vars.count(var)) {
      vars[var].state = ResourceState::Freed;
    }

    return true;
  }

  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    clang::Expr *retValue = ret->getRetValue();
    if (!retValue)
      return true;

    retValue = retValue->IgnoreParenCasts();

    if (auto *declRef = clang::dyn_cast<clang::DeclRefExpr>(retValue)) {
      if (auto *var = clang::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
        if (vars.count(var) && vars[var].state == ResourceState::Allocated) {
          vars[var].state = ResourceState::Returned;
          vars[var].loc = ret->getReturnLoc();
        }
      }
    }

    return true;
  }

  void outputs() {
    if (vars.empty())
      return;

    clang::DiagnosticsEngine &DE = m_context->getDiagnostics();

    unsigned leakDiagID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                             "%0 resource '%1' is not freed");
    unsigned returnLeakDiagID =
        DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                           "%0 resource '%1' may escape via return");

    for (auto &[var, info] : vars) {
      if (info.state == ResourceState::Allocated) {
        DE.Report(info.loc, leakDiagID) << info.type << var->getNameAsString();
      } else if (info.state == ResourceState::Returned) {
        DE.Report(info.loc, returnLeakDiagID)
            << info.type << var->getNameAsString();
      }
    }
  }

private:
  std::string getAllocationType(clang::Expr *exp) {
    clang::Expr *castexp = exp->IgnoreParenCasts();

    if (auto *call = clang::dyn_cast<clang::CallExpr>(castexp)) {
      if (auto *func = call->getDirectCallee()) {
        llvm::StringRef name = func->getName();

        if (name == "malloc")
          return "malloc";

        if (name == "fopen")
          return "file";
      }
    }

    if (clang::isa<clang::CXXNewExpr>(castexp))
      return "memory";

    return "";
  }

  clang::VarDecl *getVarDecl(clang::Expr *exp) {
    if (!exp)
      return nullptr;

    clang::Expr *castexp = exp->IgnoreParenCasts();

    if (auto *ref = clang::dyn_cast<clang::DeclRefExpr>(castexp)) {
      return clang::dyn_cast<clang::VarDecl>(ref->getDecl());
    }

    return nullptr;
  }

  clang::ASTContext *m_context;
  std::map<clang::VarDecl *, ResourceInfo> vars;
};

class NikolaevDConsumer final : public clang::ASTConsumer {
public:
  explicit NikolaevDConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.outputs();
  }

private:
  NikolaevDVisitor m_visitor;
};

class NikolaevDAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<NikolaevDConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

  ActionType getActionType() override { return AddAfterMainAction; }
};
} // namespace

static clang::FrontendPluginRegistry::Add<NikolaevDAction>
    X("nikolaev_d_analyzer_plugin", "Resource leak analyzer");
