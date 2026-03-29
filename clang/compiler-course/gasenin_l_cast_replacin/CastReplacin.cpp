#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

static std::string determineCppCast(CStyleCastExpr *expr, ASTContext &ctx) {
  const QualType dst = expr->getType();
  const QualType src = expr->getSubExpr()->getType();
  const CastKind kind = expr->getCastKind();

  switch (kind) {

  // --- reinterpret territory --------------------------------------------
  case CK_BitCast:
  case CK_LValueBitCast:
    if (dst->isVoidPointerType() || src->isVoidPointerType())
      return "static_cast";
    return "reinterpret_cast";

  case CK_IntegralToPointer:
  case CK_PointerToIntegral:
  case CK_ReinterpretMemberPointer:
    return "reinterpret_cast";

  // --- const_cast territory ---------------------------------------------
  case CK_NoOp: {
    // In Clang 21 a CStyleCastExpr may carry CK_NoOp when the real
    // conversion (e.g. FloatingToIntegral) is already encoded in a nested
    // ImplicitCastExpr.  getSubExpr()->getType() would then return the
    // *post-conversion* type, making dst == src and triggering a false
    // const_cast.  getSubExprAsWritten() strips those implicit casts and
    // gives the type the programmer actually wrote.
    const QualType srcWritten = expr->getSubExprAsWritten()->getType();
    if (dst->isPointerType() && srcWritten->isPointerType()) {
      QualType dstPointee = dst->getPointeeType().getUnqualifiedType();
      QualType srcPointee = srcWritten->getPointeeType().getUnqualifiedType();
      if (ctx.hasSameType(dstPointee, srcPointee))
        return "const_cast";
    }
    if (ctx.hasSameUnqualifiedType(dst, srcWritten))
      return "const_cast";
    return "static_cast";
  }

  // --- dynamic_cast territory -------------------------------------------
  case CK_BaseToDerived: {
    QualType srcBase = src->isPointerType() ? src->getPointeeType() : src;
    if (const auto *rd = srcBase->getAsCXXRecordDecl())
      if (rd->isPolymorphic())
        return "dynamic_cast";
    return "static_cast";
  }

  // --- static_cast territory --------------------------------------------
  case CK_DerivedToBase:
  case CK_UncheckedDerivedToBase:
  case CK_IntegralCast:
  case CK_FloatingCast:
  case CK_FloatingToIntegral:
  case CK_IntegralToFloating:
  case CK_ToVoid:
  case CK_NullToPointer:
  case CK_NullToMemberPointer:
  case CK_PointerToBoolean:
  case CK_IntegralToBoolean:
  case CK_FloatingToBoolean:
    return "static_cast";

  default:
    return "static_cast";
  }
}

class CastReplaceVisitor : public RecursiveASTVisitor<CastReplaceVisitor> {
public:
  CastReplaceVisitor(ASTContext *ctx, Rewriter &rewriter)
      : m_ctx(ctx), m_rewriter(rewriter) {}

  bool VisitCStyleCastExpr(CStyleCastExpr *cast) {
    SourceManager &SM = m_ctx->getSourceManager();

    // Skip casts that originate in system headers or macro expansions.
    if (SM.isInSystemHeader(cast->getBeginLoc()))
      return true;
    if (cast->getBeginLoc().isMacroID())
      return true;

    const std::string cppCast = determineCppCast(cast, *m_ctx);
    // getTypeAsWritten() returns the internal "_Bool" spelling for bool.
    // Use PrintingPolicy to get the canonical user-facing type name instead.
    PrintingPolicy PP(m_ctx->getLangOpts());
    PP.Bool = 1;
    const std::string typeStr = cast->getTypeAsWritten().getAsString(PP);

    // Replace "(Type)" with "cppCast<Type>("
    // i.e. the range from '(' to ')' (inclusive) becomes the new prefix.
    SourceRange parenRange(cast->getLParenLoc(), cast->getRParenLoc());
    m_rewriter.ReplaceText(parenRange, cppCast + "<" + typeStr + ">(");

    // Append closing ')' right after the sub-expression.
    // InsertTextAfterToken internally advances past the token, so pass
    // getEndLoc() directly — calling getLocForEndOfToken first would
    // double-shift the position onto the following ';'.
    m_rewriter.InsertTextAfterToken(cast->getSubExpr()->getEndLoc(), ")");

    return true;
  }

private:
  ASTContext *m_ctx;
  Rewriter &m_rewriter;
};

class CastReplaceConsumer final : public ASTConsumer {
public:
  CastReplaceConsumer(ASTContext *ctx, Rewriter &rewriter)
      : m_visitor(ctx, rewriter) {}

  void HandleTranslationUnit(ASTContext &ctx) override {
    m_visitor.TraverseDecl(ctx.getTranslationUnitDecl());
  }

private:
  CastReplaceVisitor m_visitor;
};

class CastReplaceAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &ci, llvm::StringRef /*file*/) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<CastReplaceConsumer>(&ci.getASTContext(),
                                                 m_rewriter);
  }

  bool ParseArgs(const CompilerInstance & /*ci*/,
                 const std::vector<std::string> & /*args*/) override {
    return true;
  }

  void EndSourceFileAction() override {
    SourceManager &SM = m_rewriter.getSourceMgr();
    m_rewriter.getEditBuffer(SM.getMainFileID()).write(llvm::errs());
  }

private:
  Rewriter m_rewriter;
};

} // anonymous namespace

static FrontendPluginRegistry::Add<CastReplaceAction>
    X("cast_replace_plugin",
      "Replace C-style casts with appropriate C++ casts");
