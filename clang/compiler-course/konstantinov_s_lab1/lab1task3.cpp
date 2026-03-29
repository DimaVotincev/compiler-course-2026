#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

namespace {
enum class ReplacementCastKind { Static, Const, Reinterpret };

std::optional<std::string> castKindToString(ReplacementCastKind kind) {
  switch (kind) {
  case ReplacementCastKind::Static:
    return std::string("static_cast");
  case ReplacementCastKind::Const:
    return std::string("const_cast");
  case ReplacementCastKind::Reinterpret:
    return std::string("reinterpret_cast");
  }

  return std::nullopt;
}

bool requiresConstCast(clang::QualType source, clang::QualType target) {
  if (source->isPointerType() && target->isPointerType()) {
    source = source->getPointeeType();
    target = target->getPointeeType();
  }

  const bool constChanged =
      source.isConstQualified() != target.isConstQualified();
  const bool volatileChanged =
      source.isVolatileQualified() != target.isVolatileQualified();

  return constChanged || volatileChanged;
}

bool isVoidPointerConversion(clang::QualType source, clang::QualType target) {
  const bool srcVoidPtr =
      source->isPointerType() && source->getPointeeType()->isVoidType();
  const bool dstVoidPtr =
      target->isPointerType() && target->getPointeeType()->isVoidType();

  return srcVoidPtr || dstVoidPtr;
}

ReplacementCastKind classifyCast(const clang::CStyleCastExpr *expr) {
  const clang::CastKind kind = expr->getCastKind();

  const clang::QualType sourceType = expr->getSubExpr()->getType();
  const clang::QualType targetType = expr->getType();

  if (kind == clang::CK_IntegralToPointer ||
      kind == clang::CK_PointerToIntegral ||
      kind == clang::CK_ReinterpretMemberPointer) {
    return ReplacementCastKind::Reinterpret;
  }

  if (kind == clang::CK_BitCast || kind == clang::CK_LValueBitCast ||
      kind == clang::CK_LValueToRValueBitCast) {
    if (isVoidPointerConversion(sourceType, targetType)) {
      return ReplacementCastKind::Static;
    }

    return ReplacementCastKind::Reinterpret;
  }

  if (kind == clang::CK_NoOp) {
    if (requiresConstCast(sourceType, targetType)) {
      return ReplacementCastKind::Const;
    }

    return ReplacementCastKind::Static;
  }

  return ReplacementCastKind::Static;
}

class CastRewriteVisitor final
    : public clang::RecursiveASTVisitor<CastRewriteVisitor> {
public:
  CastRewriteVisitor(clang::ASTContext *astContext, clang::Rewriter &rewriter)
      : context(astContext), sourceRewriter(rewriter) {}

  bool shouldTraversePostOrder() const { return true; }

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *expr) {
    const ReplacementCastKind replacementKind = classifyCast(expr);
    const std::optional<std::string> castKeyword =
        castKindToString(replacementKind);
    if (!castKeyword.has_value()) {
      return true;
    }
    const std::string targetType =
        expr->getTypeAsWritten().getAsString(context->getPrintingPolicy());
    clang::Expr *innerExpr = expr->getSubExprAsWritten();
    const clang::CharSourceRange innerRange =
        clang::CharSourceRange::getTokenRange(innerExpr->getSourceRange());
    const std::string innerText = sourceRewriter.getRewrittenText(innerRange);
    const std::string replacement =
        *castKeyword + "<" + targetType + ">(" + innerText + ")";
    const clang::CharSourceRange castRange =
        clang::CharSourceRange::getTokenRange(expr->getSourceRange());
    sourceRewriter.ReplaceText(castRange, replacement);
    return true;
  }

private:
  clang::ASTContext *context;
  clang::Rewriter &sourceRewriter;
};

class CastRewriteConsumer : public clang::ASTConsumer {
public:
  CastRewriteConsumer(clang::ASTContext *astContext, clang::Rewriter &rewriter)
      : visitor(astContext, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  CastRewriteVisitor visitor;
};

class CastRewritePluginAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &compiler,
                    llvm::StringRef) override {
    rewriter.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());

    return std::make_unique<CastRewriteConsumer>(&compiler.getASTContext(),
                                                 rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    clang::SourceManager &sourceManager = rewriter.getSourceMgr();
    rewriter.getEditBuffer(sourceManager.getMainFileID()).write(llvm::outs());
  }

private:
  clang::Rewriter rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<CastRewritePluginAction>
    X("cast_rewrite_plugin", "Replace C-style casts with C++ casts");
