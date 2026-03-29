#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

using namespace clang;

namespace {

class ResourceVisitor : public RecursiveASTVisitor<ResourceVisitor> {
public:
  explicit ResourceVisitor(ASTContext &ctx) : context(ctx) {}

  bool VisitVarDecl(VarDecl *vd) {
    if (!vd->hasInit())
      return true;

    Expr *init = vd->getInit()->IgnoreParenCasts();

    if (isAllocation(init)) {
      allocated.insert(vd);
    }

    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *op) {
    if (!op->isAssignmentOp())
      return true;

    Expr *rhs = op->getRHS()->IgnoreParenCasts();
    if (!isAllocation(rhs))
      return true;

    VarDecl *vd = extractVar(op->getLHS());
    if (vd)
      allocated.insert(vd);

    return true;
  }

  bool VisitCallExpr(CallExpr *call) {
    FunctionDecl *callee = call->getDirectCallee();
    if (!callee)
      return true;

    StringRef name = callee->getName();

    if (name == "free" || name == "fclose") {
      Expr *arg = call->getArg(0)->IgnoreParenCasts();
      VarDecl *vd = extractVar(arg);
      if (vd)
        released.insert(vd);
    }

    return true;
  }

  bool VisitCXXDeleteExpr(CXXDeleteExpr *expr) {
    Expr *arg = expr->getArgument()->IgnoreParenCasts();
    VarDecl *vd = extractVar(arg);
    if (vd)
      released.insert(vd);

    return true;
  }

  bool VisitReturnStmt(ReturnStmt *ret) {
    for (auto *var : allocated) {
      if (released.find(var) == released.end() &&
          reported.find(var) == reported.end()) {
        warnReturn(ret->getReturnLoc(), var);
        reported.insert(var);
      }
    }
    return true;
  }

  void finalize() {
    for (auto *var : allocated) {
      if (released.find(var) == released.end() &&
          reported.find(var) == reported.end()) {
        warnFinalize(var->getLocation(), var);
      }
    }
  }

private:
  bool isAllocation(Expr *expr) {
    if (!expr)
      return false;

    if (isa<CXXNewExpr>(expr))
      return true;

    if (auto *call = dyn_cast<CallExpr>(expr)) {
      if (auto *callee = call->getDirectCallee()) {
        StringRef name = callee->getName();
        return name == "malloc" || name == "fopen";
      }
    }

    return false;
  }

  VarDecl *extractVar(Expr *expr) {
    expr = expr->IgnoreParenCasts();

    if (auto *ref = dyn_cast<DeclRefExpr>(expr))
      return dyn_cast<VarDecl>(ref->getDecl());

    return nullptr;
  }

  void warnReturn(SourceLocation loc, VarDecl *var) {
    DiagnosticsEngine &diag = context.getDiagnostics();
    unsigned id = diag.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "ресурс для переменной '%0' может быть не освобождён при выходе");
    diag.Report(loc, id) << var->getName();
  }

  void warnFinalize(SourceLocation loc, VarDecl *var) {
    DiagnosticsEngine &diag = context.getDiagnostics();
    unsigned id = diag.getCustomDiagID(
        DiagnosticsEngine::Warning, "выделенный ресурс для '%0' не освобождён");
    diag.Report(loc, id) << var->getName();
  }

  ASTContext &context;
  std::unordered_set<VarDecl *> allocated;
  std::unordered_set<VarDecl *> released;
  std::unordered_set<VarDecl *> reported;
};

class ResourceConsumer : public ASTConsumer {
public:
  explicit ResourceConsumer(ASTContext &ctx) : visitor(ctx) {}

  void HandleTranslationUnit(ASTContext &ctx) override {
    visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    visitor.finalize();
  }

private:
  ResourceVisitor visitor;
};

class ResourcePluginAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<ResourceConsumer>(ci.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ResourcePluginAction>
    X("lifanov_k_resource-checker", "detects missing delete/free/fclose");