// yurkin_g_lab1.cpp
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseMap.h"

using namespace clang;

namespace {

enum class ResourceKind { New, Malloc, Fopen };

struct AllocInfo {
  ResourceKind kind;
  SourceLocation loc; // location to report (we use VarDecl->getLocation())
  const VarDecl *var;
  bool freed = false;
  bool reported = false; // true if we already emitted a return-site diagnostic
};

class LeakVisitor final : public RecursiveASTVisitor<LeakVisitor> {
public:
  explicit LeakVisitor(ASTContext *context) : m_context(context) {}

  // VarDecl initializers: T *p = new T; p = (T*)malloc(...); FILE *f =
  // fopen(...);
  bool VisitVarDecl(VarDecl *vd) {
    if (!vd->hasInit())
      return true;

    const Expr *init = vd->getInit();
    if (!init)
      return true;

    init = init->IgnoreParenImpCasts();
    init = init->IgnoreCasts();

    if (const auto *newExpr = dyn_cast<CXXNewExpr>(init)) {
      recordAlloc(vd, ResourceKind::New, vd->getLocation());
    } else if (const auto *call = dyn_cast<CallExpr>(init)) {
      if (const FunctionDecl *fd = call->getDirectCallee()) {
        StringRef name = fd->getName();
        if (name == "malloc") {
          recordAlloc(vd, ResourceKind::Malloc, vd->getLocation());
        } else if (name == "fopen") {
          recordAlloc(vd, ResourceKind::Fopen, vd->getLocation());
        }
      }
    }
    return true;
  }

  // Assignments: p = new T; p = malloc(...); f = fopen(...);
  bool VisitBinaryOperator(BinaryOperator *bo) {
    if (!bo->isAssignmentOp())
      return true;

    const Expr *lhs = bo->getLHS();
    const Expr *rhs = bo->getRHS();
    if (!lhs || !rhs)
      return true;

    lhs = lhs->IgnoreParenImpCasts();
    lhs = lhs->IgnoreCasts();

    rhs = rhs->IgnoreParenImpCasts();
    rhs = rhs->IgnoreCasts();

    const DeclRefExpr *dref = dyn_cast<DeclRefExpr>(lhs);
    if (!dref)
      return true;

    const ValueDecl *vd = dref->getDecl();
    const VarDecl *var = dyn_cast<VarDecl>(vd);
    if (!var)
      return true;

    if (const auto *newExpr = dyn_cast<CXXNewExpr>(rhs)) {
      recordAlloc(var, ResourceKind::New, var->getLocation());
    } else if (const auto *call = dyn_cast<CallExpr>(rhs)) {
      if (const FunctionDecl *fd = call->getDirectCallee()) {
        StringRef name = fd->getName();
        if (name == "malloc") {
          recordAlloc(var, ResourceKind::Malloc, var->getLocation());
        } else if (name == "fopen") {
          recordAlloc(var, ResourceKind::Fopen, var->getLocation());
        }
      }
    }
    return true;
  }

  // delete p;
  bool VisitCXXDeleteExpr(CXXDeleteExpr *del) {
    const Expr *op = del->getArgument();
    if (!op)
      return true;
    op = op->IgnoreParenImpCasts();
    op = op->IgnoreCasts();
    if (const DeclRefExpr *dref = dyn_cast<DeclRefExpr>(op)) {
      if (const VarDecl *var = dyn_cast<VarDecl>(dref->getDecl())) {
        markFreed(var);
      }
    }
    return true;
  }

  // free(...) and fclose(...)
  bool VisitCallExpr(CallExpr *call) {
    if (const FunctionDecl *fd = call->getDirectCallee()) {
      StringRef name = fd->getName();
      if (name == "free" || name == "fclose") {
        if (call->getNumArgs() >= 1) {
          const Expr *arg = call->getArg(0);
          if (!arg)
            return true;
          arg = arg->IgnoreParenImpCasts();
          arg = arg->IgnoreCasts();
          if (const DeclRefExpr *dref = dyn_cast<DeclRefExpr>(arg)) {
            if (const VarDecl *var = dyn_cast<VarDecl>(dref->getDecl())) {
              markFreed(var);
            }
          }
        }
      }
    }
    return true;
  }

  // Helper: find enclosing FunctionDecl for a given Stmt using ASTContext
  // parents
  const FunctionDecl *findEnclosingFunction(const Stmt *S) {
    if (!S)
      return nullptr;
    // climb parents until FunctionDecl found
    const DynTypedNode start = DynTypedNode::create(*S);
    SmallVector<DynTypedNode, 8> work;
    work.push_back(start);
    // We'll climb using getParents repeatedly
    const DynTypedNode *node = &work[0];
    // Use iterative parent climbing: repeatedly query parents of current node
    DynTypedNode current = start;
    while (true) {
      auto parents = m_context->getParents(current);
      if (parents.empty())
        break;
      // take first parent and continue
      current = parents[0];
      if (const FunctionDecl *FD = current.get<FunctionDecl>())
        return FD;
      // if parent is a DeclStmt or CompoundStmt or other Stmt, continue
      // climbing otherwise keep climbing
    }
    return nullptr;
  }

  // ReturnStmt: если возвращается переменная с незакрытой аллокацией —
  // диагностируем на return Также: если return без значения, проверяем
  // локальные аллокации в той же функции и диагностируем
  bool VisitReturnStmt(ReturnStmt *rs) {
    const Expr *ret = rs->getRetValue();
    if (ret) {
      ret = ret->IgnoreParenImpCasts();
      ret = ret->IgnoreCasts();
      if (const DeclRefExpr *dref = dyn_cast<DeclRefExpr>(ret)) {
        if (const VarDecl *var = dyn_cast<VarDecl>(dref->getDecl())) {
          auto it = m_allocs.find(var);
          if (it != m_allocs.end() && !it->second.freed &&
              !it->second.reported) {
            DiagnosticsEngine &DE = m_context->getDiagnostics();
            unsigned DiagID = DE.getCustomDiagID(
                DiagnosticsEngine::Warning,
                "Ресурс для переменной '%0' может быть не освобожден (не "
                "гарантированное освобождение при return)!");
            DE.Report(rs->getBeginLoc(), DiagID) << var->getName();
            it->second.reported = true;
          }
        }
      }
    } else {
      // return without value: early exit — check for any non-freed local
      // allocations in same function
      const FunctionDecl *FD = findEnclosingFunction(rs);
      if (!FD)
        return true;
      for (auto &p : m_allocs) {
        AllocInfo &info = p.second;
        if (info.freed || info.reported)
          continue;
        // Check if var is declared inside the same function (DeclContext chain
        // contains FD)
        const DeclContext *dc = info.var->getDeclContext();
        // climb decl contexts to see if FD is an ancestor
        const DeclContext *cur = dc;
        bool sameFunc = false;
        while (cur) {
          if (cur == FD) {
            sameFunc = true;
            break;
          }
          cur = cur->getParent();
        }
        if (!sameFunc)
          continue;
        // Also ensure the variable is declared before the return (simple source
        // order check)
        SourceManager &SM = m_context->getSourceManager();
        if (SM.isBeforeInTranslationUnit(info.var->getLocation(),
                                         rs->getBeginLoc())) {
          DiagnosticsEngine &DE = m_context->getDiagnostics();
          unsigned DiagID = DE.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "Ресурс для переменной '%0' может быть не освобожден (не "
              "гарантированное освобождение при return)!");
          DE.Report(rs->getBeginLoc(), DiagID) << info.var->getName();
          info.reported = true;
        }
      }
    }
    return true;
  }

  // После обхода TU — сообщаем о всех аллокациях, которые не были помечены как
  // freed или reported
  void reportLeaks() {
    DiagnosticsEngine &DE = m_context->getDiagnostics();
    for (const auto &p : m_allocs) {
      const AllocInfo &info = p.second;
      if (!info.freed && !info.reported) {
        unsigned DiagID = DE.getCustomDiagID(
            DiagnosticsEngine::Warning,
            "Память или ресурс для переменной '%0' не освобождены!");
        DE.Report(info.loc, DiagID) << info.var->getName();
      }
    }
  }

private:
  ASTContext *m_context;
  llvm::DenseMap<const VarDecl *, AllocInfo> m_allocs;

  void recordAlloc(const VarDecl *var, ResourceKind kind, SourceLocation loc) {
    AllocInfo info;
    info.kind = kind;
    info.loc = loc;
    info.var = var;
    info.freed = false;
    info.reported = false;
    m_allocs[var] = info;
  }

  void markFreed(const VarDecl *var) {
    auto it = m_allocs.find(var);
    if (it != m_allocs.end()) {
      it->second.freed = true;
    }
  }
};

class LeakConsumer final : public ASTConsumer {
public:
  explicit LeakConsumer(ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.reportLeaks();
  }

private:
  LeakVisitor m_visitor;
};

class LeakAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<LeakConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<LeakAction> X("leak_checker",
                                                 "TU-level leak checker");