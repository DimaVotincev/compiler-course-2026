#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

class CastTypeDeterminer {
public:
  static std::string determineCastType(CStyleCastExpr *cast) {
    CastKind kind = cast->getCastKind();
    QualType destType = cast->getType();
    Expr *subExpr = cast->getSubExpr();
    QualType srcType = subExpr->getType();

    if (isConstCastNeeded(kind, destType, srcType)) {
      return "const_cast";
    }

    if (isReinterpretCastNeeded(kind, destType, srcType)) {
      return "reinterpret_cast";
    }

    if (isDynamicCastNeeded(destType, srcType)) {
      return "dynamic_cast";
    }

    return "static_cast";
  }

private:
  static bool isConstCastNeeded(CastKind kind, QualType destType,
                                QualType srcType) {
    if (kind != CK_NoOp)
      return false;

    if (destType->isPointerType() && srcType->isPointerType()) {
      QualType destPointee = destType->getPointeeType();
      QualType srcPointee = srcType->getPointeeType();

      return (destPointee.getCVRQualifiers() !=
              srcPointee.getCVRQualifiers()) &&
             (destPointee.getTypePtr() == srcPointee.getTypePtr());
    }

    if (destType->isReferenceType() && srcType->isReferenceType()) {
      QualType destRef = destType->getPointeeType();
      QualType srcRef = srcType->getPointeeType();

      return (destRef.getCVRQualifiers() != srcRef.getCVRQualifiers()) &&
             (destRef.getTypePtr() == srcRef.getTypePtr());
    }

    return false;
  }

  static bool isReinterpretCastNeeded(CastKind kind, QualType destType,
                                      QualType srcType) {
    if (kind == CK_BitCast) {
      if (destType->isPointerType() && srcType->isPointerType()) {
        QualType destPointee = destType->getPointeeType().getUnqualifiedType();
        QualType srcPointee = srcType->getPointeeType().getUnqualifiedType();
        return destPointee.getTypePtr() != srcPointee.getTypePtr();
      }
      return true;
    }

    if (kind == CK_PointerToIntegral || kind == CK_IntegralToPointer) {
      return true;
    }

    return false;
  }

  static bool isDynamicCastNeeded(QualType destType, QualType srcType) {
    if (!destType->isPointerType() || !srcType->isPointerType()) {
      return false;
    }

    QualType destPointee = destType->getPointeeType();
    QualType srcPointee = srcType->getPointeeType();

    CXXRecordDecl *destClass = destPointee->getAsCXXRecordDecl();
    CXXRecordDecl *srcClass = srcPointee->getAsCXXRecordDecl();

    if (!destClass || !srcClass)
      return false;

    if (!srcClass->isPolymorphic())
      return false;

    if (destClass->isDerivedFrom(srcClass) ||
        srcClass->isDerivedFrom(destClass)) {
      return true;
    }

    return false;
  }
};

class CastVisitor final : public RecursiveASTVisitor<CastVisitor> {
public:
  explicit CastVisitor(ASTContext *context, Rewriter &rewriter)
      : m_context(context), m_rewriter(rewriter) {}

  bool VisitCStyleCastExpr(CStyleCastExpr *cast) {
    if (cast->getBeginLoc().isMacroID())
      return true;

    std::string castType = CastTypeDeterminer::determineCastType(cast);

    QualType targetType = cast->getTypeAsWritten();
    std::string targetTypeStr = targetType.getAsString();

    Expr *subExpr = cast->getSubExpr();

    std::string subExprStr = getExprAsString(subExpr);

    std::string replacement =
        castType + "<" + targetTypeStr + ">(" + subExprStr + ")";

    m_rewriter.ReplaceText(cast->getSourceRange(), replacement);

    return true;
  }

private:
  ASTContext *m_context;
  Rewriter &m_rewriter;

  std::string getExprAsString(Expr *expr) {
    SourceManager &sm = m_context->getSourceManager();
    SourceRange range = expr->getSourceRange();

    if (range.isInvalid())
      return "<expr>";

    const char *begin = sm.getCharacterData(range.getBegin());
    const char *end = sm.getCharacterData(range.getEnd());

    return std::string(begin, end - begin + 1);
  }

  SourceLocation getExprEndLoc(Expr *expr) {
    return Lexer::getLocForEndOfToken(expr->getEndLoc(), 0,
                                      m_context->getSourceManager(),
                                      m_context->getLangOpts());
  }
};

class CastConsumer final : public ASTConsumer {
public:
  explicit CastConsumer(ASTContext *context, Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  CastVisitor m_visitor;
};

class CastAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<CastConsumer>(&ci.getASTContext(), m_rewriter);
  }

  void EndSourceFileAction() override {
    m_rewriter.getEditBuffer(m_rewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

  bool ParseArgs(const CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

private:
  Rewriter m_rewriter;
};

} // namespace

static FrontendPluginRegistry::Add<CastAction>
    X("kazennova_a_lab1_plugin",
      "Replace C-style casts with appropriate C++ casts");