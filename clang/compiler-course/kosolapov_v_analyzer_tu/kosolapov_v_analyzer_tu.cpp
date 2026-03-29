#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <stack>
#include <vector>

namespace {

struct ResourceRecord {
  clang::SourceLocation loc;
  std::string type;
  const clang::VarDecl *var;
  const clang::Expr *allocExpr;
};

class KosolapovVAnalyzerTUVisitor
    : public clang::RecursiveASTVisitor<KosolapovVAnalyzerTUVisitor> {
public:
  explicit KosolapovVAnalyzerTUVisitor(clang::ASTContext *ctx)
      : m_ctx(ctx), diagLeakID(ctx->getDiagnostics().getCustomDiagID(
                        clang::DiagnosticsEngine::Warning,
                        "Potential leak of %0 at line %1")) {}

  bool TraverseFunctionDecl(clang::FunctionDecl *func) {
    m_stack.emplace();
    bool result = RecursiveASTVisitor::TraverseFunctionDecl(func);
    if (!m_stack.empty()) {
      for (const auto &rec : m_stack.top()) {
        unsigned line =
            m_ctx->getSourceManager().getSpellingLineNumber(rec.loc);
        m_ctx->getDiagnostics().Report(rec.loc, diagLeakID) << rec.type << line;
      }
      m_stack.pop();
    }
    return result;
  }
  bool TraverseVarDecl(clang::VarDecl *VD) {
    if (VD->hasInit()) {
      TraverseStmt(VD->getInit());
    }
    WalkUpFromVarDecl(VD);
    return true;
  }
  bool VisitCXXNewExpr(clang::CXXNewExpr *e) {
    addRecord(e->getExprLoc(), "new", nullptr, e);
    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *e) {
    if (auto *var = getVarFromExpr(e->getArgument()))
      removeRecord(var, "new");
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *callee = call->getDirectCallee();
    if (!callee)
      return true;

    llvm::StringRef name = callee->getName();
    clang::SourceLocation loc = call->getExprLoc();

    if (name == "malloc") {
      addRecord(loc, "malloc", nullptr, call);
    } else if (name == "fopen") {
      addRecord(loc, "fopen", nullptr, call);
    } else if (name == "free" && call->getNumArgs() > 0) {
      if (auto *var = getVarFromExpr(call->getArg(0)))
        removeRecord(var, "malloc");
    } else if (name == "fclose" && call->getNumArgs() > 0) {
      if (auto *var = getVarFromExpr(call->getArg(0)))
        removeRecord(var, "fopen");
    }
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *var) {
    if (!var->hasInit())
      return true;
    clang::Expr *init = var->getInit()->IgnoreParenCasts();

    if (auto *newExpr = llvm::dyn_cast<clang::CXXNewExpr>(init)) {
      removeRecordByExpr(newExpr, "new");
      addRecord(newExpr->getExprLoc(), "new", var, newExpr);
    } else if (auto *call = llvm::dyn_cast<clang::CallExpr>(init)) {
      if (auto *callee = call->getDirectCallee()) {
        llvm::StringRef name = callee->getName();
        if (name == "malloc" || name == "fopen") {
          removeRecordByExpr(call, name.str());
          addRecord(call->getExprLoc(), name.str(), var, call);
        }
      }
    }
    return true;
  }

private:
  clang::ASTContext *m_ctx;
  unsigned diagLeakID;
  std::stack<std::vector<ResourceRecord>> m_stack;

  void addRecord(clang::SourceLocation loc, const std::string &type,
                 const clang::VarDecl *var, const clang::Expr *expr) {
    if (!m_stack.empty())
      m_stack.top().push_back({loc, type, var, expr});
  }

  void removeRecord(const clang::VarDecl *var, const std::string &type) {
    if (m_stack.empty())
      return;
    auto &vec = m_stack.top();
    for (auto it = vec.begin(); it != vec.end(); ++it) {
      if (it->var == var && it->type == type) {
        vec.erase(it);
        return;
      }
    }
  }

  void removeRecordByExpr(const clang::Expr *expr, const std::string &type) {
    if (m_stack.empty())
      return;
    auto &vec = m_stack.top();
    for (auto it = vec.begin(); it != vec.end(); ++it) {
      if (it->allocExpr == expr && it->type == type) {
        vec.erase(it);
        return;
      }
    }
  }

  const clang::VarDecl *getVarFromExpr(clang::Expr *e) {
    e = e->IgnoreParenCasts();
    if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(e))
      return llvm::dyn_cast<clang::VarDecl>(declRef->getDecl());
    return nullptr;
  }
};

class KosolapovVAnalyzerTUConsumer : public clang::ASTConsumer {
public:
  explicit KosolapovVAnalyzerTUConsumer(clang::ASTContext *ctx)
      : m_visitor(ctx) {}
  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    m_visitor.TraverseDecl(ctx.getTranslationUnitDecl());
  }

private:
  KosolapovVAnalyzerTUVisitor m_visitor;
};

class KosolapovVAnalyzerTUAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<KosolapovVAnalyzerTUConsumer>(&ci.getASTContext());
  }
  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<KosolapovVAnalyzerTUAction>
    X("kosolapov_v_analyzer_tu",
      "Detect missing deallocation for new/malloc/fopen");