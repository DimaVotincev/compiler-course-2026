#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>

namespace {

class MaslovaVisitor final : public clang::RecursiveASTVisitor<MaslovaVisitor> {
public:
  explicit MaslovaVisitor(clang::ASTContext *context) : m_context(context) {}

  bool VisitVarDecl(clang::VarDecl *var) {
    if (var->hasInit() && isAllocation(var->getInit())) {
      m_activeAllocations[var] = {var->getLocation(),
                                  getResType(var->getInit())};
    }
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    if (op->isAssignmentOp() && isAllocation(op->getRHS())) {
      if (auto *var = getVarDecl(op->getLHS())) {
        m_activeAllocations[var] = {op->getOperatorLoc(),
                                    getResType(op->getRHS())};
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (auto *func = call->getDirectCallee()) {
      std::string name = func->getNameAsString();
      if (name == "free" || name == "fclose") {
        if (call->getNumArgs() > 0) {
          if (auto *var = getVarDecl(call->getArg(0))) {
            m_activeAllocations.erase(var);
          }
        }
      }
    }
    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    if (auto *var = getVarDecl(del->getArgument())) {
      m_activeAllocations.erase(var);
    }
    return true;
  }

  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    for (auto const &[var, info] : m_activeAllocations) {
      reportWarning(ret->getReturnLoc(), var->getNameAsString(), info.type);
    }
    m_activeAllocations.clear();
    return true;
  }

  void finalizeFunction() {
    for (auto const &[var, info] : m_activeAllocations) {
      reportWarning(info.loc, var->getNameAsString(), info.type);
    }
    m_activeAllocations.clear();
  }

private:
  struct AllocInfo {
    clang::SourceLocation loc;
    std::string type;
  };

  clang::ASTContext *m_context;
  std::map<const clang::VarDecl *, AllocInfo> m_activeAllocations;

  bool isAllocation(clang::Expr *e) {
    e = e->IgnoreParenCasts();
    if (auto *call = clang::dyn_cast<clang::CallExpr>(e)) {
      if (auto *func = call->getDirectCallee()) {
        std::string name = func->getNameAsString();
        return (name == "malloc" || name == "fopen");
      }
    }
    return clang::isa<clang::CXXNewExpr>(e);
  }

  std::string getResType(clang::Expr *e) {
    e = e->IgnoreParenCasts();
    if (clang::isa<clang::CXXNewExpr>(e))
      return "memory (new)";
    if (auto *call = clang::dyn_cast<clang::CallExpr>(e)) {
      if (auto *func = call->getDirectCallee()) {
        if (func->getNameAsString() == "malloc")
          return "memory (malloc)";
        if (func->getNameAsString() == "fopen")
          return "file (fopen)";
      }
    }
    return "resource";
  }

  const clang::VarDecl *getVarDecl(clang::Expr *e) {
    e = e->IgnoreParenCasts();
    if (auto *dre = clang::dyn_cast<clang::DeclRefExpr>(e)) {
      return clang::dyn_cast<clang::VarDecl>(dre->getDecl());
    }
    return nullptr;
  }

  void reportWarning(clang::SourceLocation loc, std::string varName,
                     std::string resType) {
    clang::DiagnosticsEngine &DE = m_context->getDiagnostics();
    unsigned diagID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                         "MaslovaAnalyzer: Resource '%0' for "
                                         "variable '%1' might not be released");
    DE.Report(loc, diagID) << resType << varName;
  }
};

class MaslovaConsumer final : public clang::ASTConsumer {
public:
  explicit MaslovaConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    auto *decl = context.getTranslationUnitDecl();
    for (auto *it : decl->decls()) {
      if (auto *func = clang::dyn_cast<clang::FunctionDecl>(it)) {
        if (func->hasBody()) {
          m_visitor.TraverseStmt(func->getBody());
          m_visitor.finalizeFunction();
        }
      }
    }
  }

private:
  MaslovaVisitor m_visitor;
};

class MaslovaAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<MaslovaConsumer>(&ci.getASTContext());
  }
  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<MaslovaAction>
    X("maslova_analyzer", "Анализатор выделения ресурсов");
