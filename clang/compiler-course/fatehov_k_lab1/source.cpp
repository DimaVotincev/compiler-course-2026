#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace {

struct AllocationRecord {
  enum class Source { New, Malloc, Fopen };

  Source allocationType;
  clang::SourceLocation location;
  const clang::VarDecl *variable;
  std::string variableName;

  std::string getTypeDescription() const {
    switch (allocationType) {
    case Source::New:
      return "operator new";
    case Source::Malloc:
      return "malloc/calloc";
    case Source::Fopen:
      return "fopen";
    }
    return "unknown";
  }
};

class LeakHunter : public clang::RecursiveASTVisitor<LeakHunter> {
public:
  explicit LeakHunter(clang::ASTContext *ctx) : context(ctx) {}

  bool VisitVarDecl(clang::VarDecl *varDecl) {
    if (!varDecl->hasInit())
      return true;

    clang::Expr *initializer = varDecl->getInit()->IgnoreParenCasts();
    trackAllocationIfNeeded(varDecl, initializer);
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *binOp) {
    if (!binOp->isAssignmentOp())
      return true;

    if (auto *lhs = llvm::dyn_cast<clang::DeclRefExpr>(
            binOp->getLHS()->IgnoreParenImpCasts())) {
      if (auto *var = llvm::dyn_cast<clang::VarDecl>(lhs->getDecl())) {
        clang::Expr *rhs = binOp->getRHS()->IgnoreParenCasts();
        trackAllocationIfNeeded(var, rhs);
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *callExpr) {
    if (auto *funcDecl = callExpr->getDirectCallee()) {
      std::string funcName = funcDecl->getNameInfo().getName().getAsString();

      if (funcName == "delete" || funcName == "free" || funcName == "fclose") {
        if (callExpr->getNumArgs() > 0) {
          if (auto *arg = llvm::dyn_cast<clang::DeclRefExpr>(
                  callExpr->getArg(0)->IgnoreParenImpCasts())) {
            if (auto *var = llvm::dyn_cast<clang::VarDecl>(arg->getDecl())) {
              freedResources.insert(var);
            }
          }
        }
      }
    }
    return true;
  }

  void performAnalysis() {
    for (const auto &[variable, record] : allocations) {
      if (freedResources.find(variable) == freedResources.end()) {
        reportLeak(record);
      }
    }
  }

private:
  clang::ASTContext *context;
  std::unordered_map<const clang::VarDecl *, AllocationRecord> allocations;
  std::unordered_set<const clang::VarDecl *> freedResources;

  void trackAllocationIfNeeded(const clang::VarDecl *var, clang::Expr *expr) {
    if (auto *newExpr = llvm::dyn_cast<clang::CXXNewExpr>(expr)) {
      allocations[var] = {AllocationRecord::Source::New, expr->getBeginLoc(),
                          var, var->getNameAsString()};
    } else if (auto *call = llvm::dyn_cast<clang::CallExpr>(expr)) {
      if (auto *func = call->getDirectCallee()) {
        std::string name = func->getNameInfo().getName().getAsString();
        if (name == "malloc" || name == "calloc") {
          allocations[var] = {AllocationRecord::Source::Malloc,
                              expr->getBeginLoc(), var, var->getNameAsString()};
        } else if (name == "fopen") {
          allocations[var] = {AllocationRecord::Source::Fopen,
                              expr->getBeginLoc(), var, var->getNameAsString()};
        }
      }
    }
  }

  void reportLeak(const AllocationRecord &record) {
    clang::SourceManager &srcMgr = context->getSourceManager();
    clang::SourceLocation loc = srcMgr.getSpellingLoc(record.location);

    if (loc.isInvalid())
      return;

    unsigned line = srcMgr.getSpellingLineNumber(loc);
    std::string filename = srcMgr.getFilename(loc).str();

    llvm::errs() << "[LEAK DETECTED] Variable '" << record.variableName
                 << "' allocated with " << record.getTypeDescription() << " at "
                 << filename << ":" << line
                 << " has no corresponding deallocation\n";
  }
};

class LeakConsumer : public clang::ASTConsumer {
public:
  explicit LeakConsumer(clang::ASTContext *ctx) : hunter(ctx) {}

  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    hunter.TraverseDecl(ctx.getTranslationUnitDecl());
    hunter.performAnalysis();
  }

private:
  LeakHunter hunter;
};

class LeakPluginAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef) override {
    return std::make_unique<LeakConsumer>(&compiler.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<LeakPluginAction>
    Register("fatehov_resource_leak", "Resource leak detector plugin");