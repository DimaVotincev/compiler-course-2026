#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace {

enum class AllocationKind {
  New,
  Malloc,
  Fopen,
};

enum class ReleaseKind {
  Delete,
  Free,
  Fclose,
};

struct AllocationInfo {
  const clang::Expr *expr = nullptr;
  const clang::ValueDecl *owner = nullptr;
  AllocationKind kind = AllocationKind::New;
  clang::SourceLocation location;
  bool released = false;
};

class ResourceLeakAnalyzer {
public:
  explicit ResourceLeakAnalyzer(clang::ASTContext &context)
      : context(context) {}

  void analyze() {
    visitDecl(context.getTranslationUnitDecl());
    emitWarnings();
  }

private:
  clang::ASTContext &context;
  std::vector<AllocationInfo> allocations;
  llvm::DenseMap<const clang::Expr *, std::size_t> allocationIndicesByExpr;
  llvm::DenseMap<const clang::ValueDecl *, std::vector<std::size_t>>
      outstandingAllocationsByOwner;

  static bool isCompatible(AllocationKind allocationKind,
                           ReleaseKind releaseKind) {
    switch (allocationKind) {
    case AllocationKind::New:
      return releaseKind == ReleaseKind::Delete;
    case AllocationKind::Malloc:
      return releaseKind == ReleaseKind::Free;
    case AllocationKind::Fopen:
      return releaseKind == ReleaseKind::Fclose;
    }

    return false;
  }

  static const char *getResourceName(AllocationKind allocationKind) {
    switch (allocationKind) {
    case AllocationKind::New:
    case AllocationKind::Malloc:
      return "memory";
    case AllocationKind::Fopen:
      return "file descriptor";
    }

    return "resource";
  }

  static const char *getAllocationName(AllocationKind allocationKind) {
    switch (allocationKind) {
    case AllocationKind::New:
      return "new";
    case AllocationKind::Malloc:
      return "malloc";
    case AllocationKind::Fopen:
      return "fopen";
    }

    return "resource";
  }

  const clang::Expr *skipTransparentWrappers(const clang::Expr *expr) const {
    const clang::Expr *current = expr;
    while (current != nullptr) {
      const clang::Expr *stripped = current->IgnoreParenImpCasts();
      if (stripped != current) {
        current = stripped;
        continue;
      }

      if (const auto *cleanups =
              clang::dyn_cast<clang::ExprWithCleanups>(current)) {
        current = cleanups->getSubExpr();
        continue;
      }

      if (const auto *bindTemporary =
              clang::dyn_cast<clang::CXXBindTemporaryExpr>(current)) {
        current = bindTemporary->getSubExpr();
        continue;
      }

      if (const auto *materialize =
              clang::dyn_cast<clang::MaterializeTemporaryExpr>(current)) {
        current = materialize->getSubExpr();
        continue;
      }

      return current;
    }

    return expr;
  }

  bool shouldTrack(const clang::Expr *expr) const {
    const clang::SourceLocation location =
        context.getSourceManager().getExpansionLoc(expr->getExprLoc());
    return location.isValid() &&
           !context.getSourceManager().isInSystemHeader(location);
  }

  std::optional<AllocationKind>
  classifyAllocation(const clang::CallExpr *callExpr) const {
    const clang::FunctionDecl *callee = callExpr->getDirectCallee();
    if (callee == nullptr || callee->getIdentifier() == nullptr) {
      return std::nullopt;
    }

    const llvm::StringRef name = callee->getName();
    if (name == "malloc") {
      return AllocationKind::Malloc;
    }

    if (name == "fopen") {
      return AllocationKind::Fopen;
    }

    return std::nullopt;
  }

  std::optional<ReleaseKind>
  classifyRelease(const clang::CallExpr *callExpr) const {
    if (callExpr->getNumArgs() == 0) {
      return std::nullopt;
    }

    const clang::FunctionDecl *callee = callExpr->getDirectCallee();
    if (callee == nullptr || callee->getIdentifier() == nullptr) {
      return std::nullopt;
    }

    const llvm::StringRef name = callee->getName();
    if (name == "free") {
      return ReleaseKind::Free;
    }

    if (name == "fclose") {
      return ReleaseKind::Fclose;
    }

    return std::nullopt;
  }

  const clang::ValueDecl *extractReferencedDecl(const clang::Expr *expr) const {
    const clang::Expr *baseExpr = skipTransparentWrappers(expr);
    if (const auto *declRefExpr =
            clang::dyn_cast<clang::DeclRefExpr>(baseExpr)) {
      return clang::dyn_cast<clang::ValueDecl>(declRefExpr->getDecl());
    }

    return nullptr;
  }

  bool referencesSameOwner(const clang::Expr *expr,
                           const clang::ValueDecl *owner) const {
    return extractReferencedDecl(expr) == owner;
  }

  void visitDecl(const clang::Decl *decl) {
    if (decl == nullptr) {
      return;
    }

    if (const auto *declContext = clang::dyn_cast<clang::DeclContext>(decl)) {
      for (const clang::Decl *child : declContext->decls()) {
        visitDecl(child);
      }
    }

    if (const auto *varDecl = clang::dyn_cast<clang::VarDecl>(decl)) {
      if (const clang::Expr *init = varDecl->getInit()) {
        visitExpr(init);
        bindOwner(init, varDecl);
      }
      return;
    }

    if (const auto *functionDecl = clang::dyn_cast<clang::FunctionDecl>(decl)) {
      if (const clang::Stmt *body = functionDecl->getBody()) {
        visitStmt(body);
      }
    }
  }

  void visitExpr(const clang::Expr *expr) { visitStmt(expr); }

  void visitStmt(const clang::Stmt *stmt) {
    if (stmt == nullptr) {
      return;
    }

    if (const auto *declStmt = clang::dyn_cast<clang::DeclStmt>(stmt)) {
      for (const clang::Decl *decl : declStmt->decls()) {
        visitDecl(decl);
      }
      return;
    }

    if (const auto *binaryOperator =
            clang::dyn_cast<clang::BinaryOperator>(stmt)) {
      if (binaryOperator->getOpcode() == clang::BO_Assign) {
        visitAssignment(binaryOperator);
        return;
      }
    }

    if (const auto *deleteExpr = clang::dyn_cast<clang::CXXDeleteExpr>(stmt)) {
      if (const clang::Expr *argument = deleteExpr->getArgument()) {
        visitExpr(argument);
        handleRelease(argument, ReleaseKind::Delete);
      }
      return;
    }

    if (const auto *newExpr = clang::dyn_cast<clang::CXXNewExpr>(stmt)) {
      for (const clang::Stmt *child : newExpr->children()) {
        visitStmt(child);
      }
      recordAllocation(newExpr, AllocationKind::New);
      return;
    }

    if (const auto *callExpr = clang::dyn_cast<clang::CallExpr>(stmt)) {
      visitCallExpr(callExpr);
      return;
    }

    for (const clang::Stmt *child : stmt->children()) {
      visitStmt(child);
    }
  }

  void visitAssignment(const clang::BinaryOperator *binaryOperator) {
    visitExpr(binaryOperator->getLHS());

    const clang::ValueDecl *owner =
        extractReferencedDecl(binaryOperator->getLHS());
    if (owner != nullptr &&
        !referencesSameOwner(binaryOperator->getRHS(), owner)) {
      invalidateOwner(owner);
    }

    visitExpr(binaryOperator->getRHS());
    if (owner != nullptr) {
      bindOwner(binaryOperator->getRHS(), owner);
    }
  }

  void visitCallExpr(const clang::CallExpr *callExpr) {
    for (const clang::Stmt *child : callExpr->children()) {
      visitStmt(child);
    }

    const std::optional<AllocationKind> allocationKind =
        classifyAllocation(callExpr);
    if (allocationKind) {
      recordAllocation(callExpr, *allocationKind);
      return;
    }

    const std::optional<ReleaseKind> releaseKind = classifyRelease(callExpr);
    if (releaseKind) {
      handleRelease(callExpr->getArg(0), *releaseKind);
    }
  }

  void recordAllocation(const clang::Expr *expr,
                        AllocationKind allocationKind) {
    if (!shouldTrack(expr)) {
      return;
    }

    const clang::Expr *key = skipTransparentWrappers(expr);
    if (allocationIndicesByExpr.find(key) != allocationIndicesByExpr.end()) {
      return;
    }

    AllocationInfo info;
    info.expr = key;
    info.kind = allocationKind;
    info.location =
        context.getSourceManager().getExpansionLoc(expr->getExprLoc());

    const std::size_t index = allocations.size();
    allocations.push_back(info);
    allocationIndicesByExpr[key] = index;
  }

  void bindOwner(const clang::Expr *expr, const clang::ValueDecl *owner) {
    const clang::Expr *key = skipTransparentWrappers(expr);
    auto it = allocationIndicesByExpr.find(key);
    if (it == allocationIndicesByExpr.end()) {
      return;
    }

    AllocationInfo &allocation = allocations[it->second];
    if (allocation.owner == owner) {
      return;
    }

    if (allocation.owner != nullptr) {
      removeOwnerAllocation(allocation.owner, it->second);
    }

    allocation.owner = owner;
    if (!allocation.released) {
      std::vector<std::size_t> &indices = outstandingAllocationsByOwner[owner];
      if (std::find(indices.begin(), indices.end(), it->second) ==
          indices.end()) {
        indices.push_back(it->second);
      }
    }
  }

  void handleRelease(const clang::Expr *releasedExpr, ReleaseKind releaseKind) {
    if (!shouldTrack(releasedExpr)) {
      return;
    }

    if (const clang::ValueDecl *owner = extractReferencedDecl(releasedExpr)) {
      markReleasedByOwner(owner, releaseKind);
      return;
    }

    markReleasedByExpr(releasedExpr, releaseKind);
  }

  void invalidateOwner(const clang::ValueDecl *owner) {
    outstandingAllocationsByOwner.erase(owner);
  }

  void markReleasedByExpr(const clang::Expr *expr, ReleaseKind releaseKind) {
    const clang::Expr *key = skipTransparentWrappers(expr);
    auto it = allocationIndicesByExpr.find(key);
    if (it == allocationIndicesByExpr.end()) {
      return;
    }

    AllocationInfo &allocation = allocations[it->second];
    if (allocation.released || !isCompatible(allocation.kind, releaseKind)) {
      return;
    }

    allocation.released = true;
    if (allocation.owner != nullptr) {
      removeOwnerAllocation(allocation.owner, it->second);
    }
  }

  void markReleasedByOwner(const clang::ValueDecl *owner,
                           ReleaseKind releaseKind) {
    auto ownerIt = outstandingAllocationsByOwner.find(owner);
    if (ownerIt == outstandingAllocationsByOwner.end()) {
      return;
    }

    std::vector<std::size_t> &ownerAllocations = ownerIt->second;
    for (auto it = ownerAllocations.rbegin(); it != ownerAllocations.rend();
         ++it) {
      AllocationInfo &allocation = allocations[*it];
      if (allocation.released || !isCompatible(allocation.kind, releaseKind)) {
        continue;
      }

      allocation.released = true;
      ownerAllocations.erase(std::next(it).base());
      if (ownerAllocations.empty()) {
        outstandingAllocationsByOwner.erase(ownerIt);
      }
      return;
    }
  }

  void removeOwnerAllocation(const clang::ValueDecl *owner, std::size_t index) {
    auto ownerIt = outstandingAllocationsByOwner.find(owner);
    if (ownerIt == outstandingAllocationsByOwner.end()) {
      return;
    }

    std::vector<std::size_t> &ownerAllocations = ownerIt->second;
    ownerAllocations.erase(
        std::remove(ownerAllocations.begin(), ownerAllocations.end(), index),
        ownerAllocations.end());
    if (ownerAllocations.empty()) {
      outstandingAllocationsByOwner.erase(ownerIt);
    }
  }

  void emitWarnings() {
    clang::DiagnosticsEngine &diagnostics = context.getDiagnostics();
    const unsigned diagnosticID = diagnostics.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "resource leak: %0 allocated with '%1' is not guaranteed to be "
        "released in this translation unit");

    for (const AllocationInfo &allocation : allocations) {
      if (allocation.released) {
        continue;
      }

      diagnostics.Report(allocation.location, diagnosticID)
          << getResourceName(allocation.kind)
          << getAllocationName(allocation.kind);
    }
  }
};

class ResourceLeakConsumer : public clang::ASTConsumer {
public:
  explicit ResourceLeakConsumer(clang::ASTContext &context)
      : analyzer(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    analyzer.analyze();
  }

private:
  ResourceLeakAnalyzer analyzer;
};

class ResourceLeakAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef) override {
    return std::make_unique<ResourceLeakConsumer>(compiler.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

static clang::FrontendPluginRegistry::Add<ResourceLeakAction>
    X("gusev_d_lab1_plugin",
      "Warn about resources that are not guaranteed to be released");

} // namespace
