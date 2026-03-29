#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace {

const clang::Expr *IgnoreWrappers(const clang::Expr *expr) {
  if (expr == nullptr) {
    return nullptr;
  }
  return expr->IgnoreParenImpCasts();
}

bool IsDeclRefToVar(const clang::Expr *expr, const clang::VarDecl *var_decl) {
  expr = IgnoreWrappers(expr);
  if (expr == nullptr) {
    return false;
  }

  const auto *decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expr);
  return decl_ref != nullptr && decl_ref->getDecl() == var_decl;
}

bool IsTargetObjectExpr(const clang::Expr *expr,
                        const clang::VarDecl *var_decl) {
  expr = IgnoreWrappers(expr);
  if (expr == nullptr) {
    return false;
  }

  if (IsDeclRefToVar(expr, var_decl)) {
    return true;
  }

  if (const auto *unary_op = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
    if (unary_op->getOpcode() == clang::UO_Deref) {
      return IsDeclRefToVar(unary_op->getSubExpr(), var_decl);
    }
  }

  if (const auto *member_expr = llvm::dyn_cast<clang::MemberExpr>(expr)) {
    return IsTargetObjectExpr(member_expr->getBase(), var_decl);
  }

  if (const auto *array_expr =
          llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
    return IsTargetObjectExpr(array_expr->getBase(), var_decl);
  }

  return false;
}

bool IsNonConstReference(clang::QualType type) {
  return type->isReferenceType() &&
         !type.getNonReferenceType().isConstQualified();
}

bool IsPointerToNonConst(clang::QualType type) {
  return type->isPointerType() && !type->getPointeeType().isConstQualified();
}

class MutationAnalyzer : public clang::RecursiveASTVisitor<MutationAnalyzer> {
public:
  explicit MutationAnalyzer(const clang::VarDecl *target) : target_(target) {}

  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    if (!op->isAssignmentOp()) {
      return true;
    }

    const clang::Expr *lhs = op->getLHS();

    if (IsDeclRefToVar(lhs, target_)) {
      variable_modified_ = true;
    }

    if (IsTargetObjectExpr(lhs, target_)) {
      object_modified_ = true;
    }

    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *op) {
    if (!op->isIncrementDecrementOp()) {
      return true;
    }

    const clang::Expr *sub_expr = op->getSubExpr();

    if (IsDeclRefToVar(sub_expr, target_)) {
      variable_modified_ = true;
    }

    if (IsTargetObjectExpr(sub_expr, target_)) {
      object_modified_ = true;
    }

    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    const auto *callee = call->getDirectCallee();
    if (callee == nullptr) {
      return true;
    }

    const unsigned arg_count = call->getNumArgs();
    const unsigned param_count = callee->getNumParams();
    const unsigned count = arg_count < param_count ? arg_count : param_count;

    for (unsigned i = 0; i < count; ++i) {
      const clang::Expr *arg = call->getArg(i);
      const clang::ParmVarDecl *param = callee->getParamDecl(i);
      const clang::QualType param_type = param->getType();

      if (IsDeclRefToVar(arg, target_)) {
        if (IsNonConstReference(param_type) ||
            IsPointerToNonConst(param_type)) {
          variable_modified_ = true;
        }
      }

      if (IsTargetObjectExpr(arg, target_)) {
        if (IsNonConstReference(param_type) ||
            IsPointerToNonConst(param_type)) {
          object_modified_ = true;
        }
      }
    }

    return true;
  }

  bool IsVariableModified() const { return variable_modified_; }
  bool IsObjectModified() const { return object_modified_; }

private:
  const clang::VarDecl *target_;
  bool variable_modified_ = false;
  bool object_modified_ = false;
};

class ConstQualifierVisitor
    : public clang::RecursiveASTVisitor<ConstQualifierVisitor> {
public:
  ConstQualifierVisitor(clang::ASTContext *context, clang::Rewriter &rewriter)
      : context_(context), rewriter_(rewriter) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func_decl) {
    if (func_decl == nullptr || !func_decl->hasBody()) {
      return true;
    }

    clang::Stmt *body = func_decl->getBody();

    for (clang::ParmVarDecl *param : func_decl->parameters()) {
      AnalyzeVar(param, body);
    }

    return true;
  }

  bool VisitVarDecl(clang::VarDecl *var_decl) {
    if (var_decl == nullptr || !var_decl->isLocalVarDecl()) {
      return true;
    }

    const auto *function_decl =
        llvm::dyn_cast_or_null<clang::FunctionDecl>(var_decl->getDeclContext());
    if (function_decl == nullptr || !function_decl->hasBody()) {
      return true;
    }

    AnalyzeVar(var_decl, function_decl->getBody());
    return true;
  }

private:
  void AnalyzeVar(clang::VarDecl *var_decl, clang::Stmt *body) {
    const clang::QualType type = var_decl->getType();

    if (type->isReferenceType()) {
      HandleReference(var_decl, body, type);
      return;
    }

    if (type->isPointerType()) {
      HandlePointer(var_decl, body, type);
    }
  }

  void HandleReference(clang::VarDecl *var_decl, clang::Stmt *body,
                       clang::QualType type) {
    if (type.getNonReferenceType().isConstQualified()) {
      return;
    }

    MutationAnalyzer analyzer(var_decl);
    analyzer.TraverseStmt(body);

    if (!analyzer.IsObjectModified()) {
      InsertConstBeforeType(var_decl);
    }
  }

  void HandlePointer(clang::VarDecl *var_decl, clang::Stmt *body,
                     clang::QualType type) {
    const bool pointee_const = type->getPointeeType().isConstQualified();
    const bool pointer_const = type.isConstQualified();

    MutationAnalyzer analyzer(var_decl);
    analyzer.TraverseStmt(body);

    if (!analyzer.IsObjectModified() && !pointee_const) {
      InsertConstBeforeType(var_decl);
    }

    if (!analyzer.IsVariableModified() && !pointer_const) {
      InsertConstBeforeVariableName(var_decl);
    }
  }

  void InsertConstBeforeType(const clang::VarDecl *var_decl) {
    const clang::TypeSourceInfo *type_info = var_decl->getTypeSourceInfo();
    if (type_info == nullptr) {
      return;
    }

    clang::TypeLoc type_loc = type_info->getTypeLoc();
    clang::SourceLocation begin = type_loc.getBeginLoc();

    if (begin.isInvalid() ||
        context_->getSourceManager().isInSystemHeader(begin)) {
      return;
    }

    rewriter_.InsertText(begin, "const ", true, true);
  }

  void InsertConstBeforeVariableName(const clang::VarDecl *var_decl) {
    clang::SourceLocation loc = var_decl->getLocation();

    if (loc.isInvalid() || context_->getSourceManager().isInSystemHeader(loc)) {
      return;
    }

    rewriter_.InsertText(loc, "const ", true, true);
  }

  clang::ASTContext *context_;
  clang::Rewriter &rewriter_;
};

class ConstQualifierConsumer : public clang::ASTConsumer {
public:
  ConstQualifierConsumer(clang::ASTContext *context, clang::Rewriter &rewriter)
      : visitor_(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    visitor_.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  ConstQualifierVisitor visitor_;
};

class ConstQualifierAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler_instance,
                    llvm::StringRef) override {
    rewriter_.setSourceMgr(compiler_instance.getSourceManager(),
                           compiler_instance.getLangOpts());
    return std::make_unique<ConstQualifierConsumer>(
        &compiler_instance.getASTContext(), rewriter_);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    const auto &source_manager = rewriter_.getSourceMgr();
    const clang::FileID main_file = source_manager.getMainFileID();

    const llvm::RewriteBuffer *rewrite_buffer =
        rewriter_.getRewriteBufferFor(main_file);

    if (rewrite_buffer != nullptr) {
      llvm::outs() << std::string(rewrite_buffer->begin(),
                                  rewrite_buffer->end());
    } else {
      llvm::outs() << source_manager.getBufferData(main_file);
    }
  }

private:
  clang::Rewriter rewriter_;
};

} // namespace

static clang::FrontendPluginRegistry::Add<ConstQualifierAction>
    X("example_plugin", "Add const to references and pointers when possible");