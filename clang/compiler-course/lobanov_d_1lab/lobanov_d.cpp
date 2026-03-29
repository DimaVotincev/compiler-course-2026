#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class ExceptionAnalyzer {
private:
  struct FunctionRecord {
    bool isProcessed = false;
    bool canThrow = false;
    llvm::SmallVector<clang::FunctionDecl *, 8> invokeList;
  };

  llvm::DenseMap<clang::FunctionDecl *, FunctionRecord> registry;
  llvm::SmallPtrSet<clang::FunctionDecl *, 32> visitStack;

  void buildCallGraph(clang::Stmt *body, clang::FunctionDecl *caller) {
    if (!body)
      return;

    if (auto *call = llvm::dyn_cast<clang::CallExpr>(body)) {
      if (auto *callee = call->getDirectCallee()) {
        registry[caller].invokeList.push_back(callee);
      }
    }

    for (auto *child : body->children()) {
      buildCallGraph(child, caller);
    }
  }

  bool containsThrow(clang::Stmt *stmt) {
    if (!stmt)
      return false;

    if (llvm::isa<clang::CXXThrowExpr>(stmt) ||
        llvm::isa<clang::CXXNewExpr>(stmt)) {
      return true;
    }

    for (auto *child : stmt->children()) {
      if (containsThrow(child))
        return true;
    }
    return false;
  }

  bool evaluate(clang::FunctionDecl *func) {
    if (!func || !func->hasBody())
      return true;

    auto &record = registry[func];
    if (record.isProcessed)
      return record.canThrow;
    if (!visitStack.insert(func).second)
      return false;

    if (auto *proto = func->getType()->getAs<clang::FunctionProtoType>()) {
      if (proto->getExceptionSpecType() != clang::EST_None) {
        record.canThrow =
            (proto->getExceptionSpecType() != clang::EST_BasicNoexcept);
        record.isProcessed = true;
        visitStack.erase(func);
        return record.canThrow;
      }
    }

    record.canThrow = containsThrow(func->getBody());

    if (!record.canThrow) {
      buildCallGraph(func->getBody(), func);

      for (auto *callee : record.invokeList) {
        if (evaluate(callee)) {
          record.canThrow = true;
          break;
        }
      }
    }

    record.isProcessed = true;
    visitStack.erase(func);
    return record.canThrow;
  }

public:
  bool mayThrow(clang::FunctionDecl *func) { return evaluate(func); }
};

class NoexceptVisitor final
    : public clang::RecursiveASTVisitor<NoexceptVisitor> {
public:
  explicit NoexceptVisitor(clang::ASTContext *context)
      : m_context(context), m_sourceManager(context->getSourceManager()) {}

  bool VisitFunctionDecl(clang::FunctionDecl *function) {
    if (!isEligible(function))
      return true;

    auto *proto = function->getType()->getAs<clang::FunctionProtoType>();
    if (!proto || proto->getExceptionSpecType() != clang::EST_None) {
      return true;
    }

    if (!m_analyzer.mayThrow(function)) {
      applyNoexcept(function);
    }

    return true;
  }

private:
  bool isEligible(clang::FunctionDecl *function) {
    if (!function->hasBody())
      return false;
    if (function->isImplicit() || function->isDeleted() ||
        function->isDefaulted())
      return false;
    if (m_sourceManager.isInSystemHeader(function->getLocation()))
      return false;

    clang::SourceLocation loc = function->getLocation();
    if (loc.isInvalid())
      return false;

    return true;
  }

  void applyNoexcept(clang::FunctionDecl *function) {
    auto *proto = function->getType()->getAs<clang::FunctionProtoType>();

    clang::FunctionProtoType::ExtProtoInfo extensionInfo =
        proto->getExtProtoInfo();
    extensionInfo.ExceptionSpec.Type = clang::EST_BasicNoexcept;

    clang::QualType updatedType = m_context->getFunctionType(
        proto->getReturnType(), proto->getParamTypes(), extensionInfo);

    function->setType(updatedType);
    llvm::outs() << "Added noexcept to: " << function->getNameAsString()
                 << "\n";
  }

  clang::ASTContext *m_context;
  clang::SourceManager &m_sourceManager;
  ExceptionAnalyzer m_analyzer;
};

class NoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit NoexceptConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  NoexceptVisitor m_visitor;
};

class NoexceptAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &instance,
                    llvm::StringRef) override {
    return std::make_unique<NoexceptConsumer>(&instance.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<NoexceptAction>
    X("add-noexcept", "Add noexcept to non-throwing functions");