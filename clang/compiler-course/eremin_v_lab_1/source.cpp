#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>

namespace {

struct ResourceInfo {
  std::string type;
  std::string varName;
  clang::SourceLocation loc;
  bool released = false;
  const clang::VarDecl *varDecl = nullptr;
};
class FunctionResourceVisitor
    : public clang::RecursiveASTVisitor<FunctionResourceVisitor> {
public:
  explicit FunctionResourceVisitor(clang::ASTContext *ctx) : context(ctx) {}

  bool VisitVarDecl(clang::VarDecl *var) {
    if (!var->hasInit())
      return true;

    clang::Expr *init = var->getInit()->IgnoreParenCasts();
    tryRegisterAlloc(var, init);
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *binOp) {
    if (!binOp->isAssignmentOp())
      return true;

    auto *lhs =
        llvm::dyn_cast<clang::DeclRefExpr>(binOp->getLHS()->IgnoreParenCasts());
    if (!lhs)
      return true;

    auto *var = llvm::dyn_cast<clang::VarDecl>(lhs->getDecl());
    if (!var)
      return true;

    clang::Expr *rhs = binOp->getRHS()->IgnoreParenCasts();
    tryRegisterAlloc(var, rhs);
    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    auto *expr = llvm::dyn_cast<clang::DeclRefExpr>(
        del->getArgument()->IgnoreParenCasts());
    if (!expr)
      return true;

    auto *var = llvm::dyn_cast<clang::VarDecl>(expr->getDecl());
    if (!var)
      return true;

    releaseResource(var, del->isArrayForm() ? "new[]" : "new");
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    auto *calleeExpr = call->getCallee()->IgnoreParenCasts();
    auto *dre = llvm::dyn_cast<clang::DeclRefExpr>(calleeExpr);
    if (!dre)
      return true;

    std::string funcName = dre->getDecl()->getNameAsString();

    if (funcName != "free" && funcName != "fclose")
      return true;

    if (call->getNumArgs() == 0)
      return true;

    auto *argDRE =
        llvm::dyn_cast<clang::DeclRefExpr>(call->getArg(0)->IgnoreParenCasts());
    if (!argDRE)
      return true;

    auto *var = llvm::dyn_cast<clang::VarDecl>(argDRE->getDecl());
    if (!var)
      return true;

    if (funcName == "free")
      releaseResource(var, "malloc");
    else // fclose
      releaseResource(var, "fopen");

    return true;
  }

  void reportLeaks() {
    clang::SourceManager &SM = context->getSourceManager();
    for (auto &r : resources) {
      if (!r.released) {
        clang::PresumedLoc ploc = SM.getPresumedLoc(r.loc);
        llvm::errs() << "Warning: resource '" << r.type << "' not released at "
                     << ploc.getFilename() << ":" << ploc.getLine() << "\n";
      }
    }
  }

private:
  clang::ASTContext *context;
  std::vector<ResourceInfo> resources;

  void tryRegisterAlloc(const clang::VarDecl *var, clang::Expr *expr) {

    if (auto *newExpr = llvm::dyn_cast<clang::CXXNewExpr>(expr)) {
      addResource(var, newExpr->isArray() ? "new[]" : "new",
                  newExpr->getBeginLoc());
      return;
    }

    auto *call = llvm::dyn_cast<clang::CallExpr>(expr);
    if (!call)
      return;

    auto *calleeExpr = call->getCallee()->IgnoreParenCasts();
    auto *dre = llvm::dyn_cast<clang::DeclRefExpr>(calleeExpr);
    if (!dre)
      return;

    std::string funcName = dre->getDecl()->getNameAsString();

    if (funcName == "malloc" || funcName == "calloc")
      addResource(var, "malloc", call->getBeginLoc());
    else if (funcName == "fopen")
      addResource(var, "fopen", call->getBeginLoc());
  }

  void addResource(const clang::VarDecl *var, const std::string &type,
                   clang::SourceLocation loc) {
    ResourceInfo r;
    r.type = type;
    r.varDecl = var;
    r.varName = var->getNameAsString();
    r.loc = loc;
    resources.push_back(r);
  }

  void releaseResource(const clang::VarDecl *var, const std::string &type) {
    for (auto &r : resources) {
      if (r.varDecl == var && r.type == type && !r.released) {
        r.released = true;
        break;
      }
    }
  }
};

class ResourceASTConsumer : public clang::ASTConsumer {
public:
  explicit ResourceASTConsumer(clang::ASTContext *ctx) : context(ctx) {}

  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    for (auto *decl : ctx.getTranslationUnitDecl()->decls()) {

      if (auto *fd = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        processFunctionDecl(fd);
      }

      else if (auto *ftd = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
        processFunctionDecl(ftd->getTemplatedDecl());
      }
    }
  }

private:
  clang::ASTContext *context;

  void processFunctionDecl(clang::FunctionDecl *func) {
    if (!func || !func->hasBody())
      return;

    llvm::errs() << func->getNameAsString() << "\n";

    FunctionResourceVisitor visitor(context);
    visitor.TraverseStmt(func->getBody());
    visitor.reportLeaks();
  }
};

class ResourcePluginAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<ResourceASTConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<ResourcePluginAction>
    X("eremin_v_lab_1_resource_checker",
      "Detects unreleased resources: new/delete, new[]/delete[], "
      "malloc/free, fopen/fclose");