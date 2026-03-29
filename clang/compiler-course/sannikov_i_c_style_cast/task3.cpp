#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>

namespace {

enum class CastStyles { Static, Const, Reinterpret, Dynamic };

llvm::StringRef getCastStyleName(CastStyles style) {
  switch (style) {
  case CastStyles::Static:
    return "static_cast";
  case CastStyles::Const:
    return "const_cast";
  case CastStyles::Reinterpret:
    return "reinterpret_cast";
  case CastStyles::Dynamic:
    return "dynamic_cast";
  }
  return "static_cast";
}

class TypeCastClassifier {
public:
  explicit TypeCastClassifier(clang::ASTContext &context)
      : m_context(context) {}

  CastStyles chooseCast(const clang::CStyleCastExpr *cast) const {
    const clang::Expr *sub = cast->getSubExpr();
    if (!sub) {
      return CastStyles::Static;
    }

    const clang::QualType sourceType = sub->getType().getCanonicalType();
    const clang::QualType targetType =
        cast->getTypeAsWritten().getCanonicalType();

    if (isCvAdjustment(sourceType, targetType)) {
      return CastStyles::Const;
    }
    if (isPolymorphicDowncast(sourceType, targetType)) {
      return CastStyles::Dynamic;
    }
    if (isPointerIntegerMix(sourceType, targetType)) {
      return CastStyles::Reinterpret;
    }
    if (isOpaquePointerConversion(sourceType, targetType)) {
      return CastStyles::Static;
    }
    if (isUnrelatedPointerConversion(sourceType, targetType)) {
      return CastStyles::Reinterpret;
    }
    if (isNumLike(sourceType) && isNumLike(targetType)) {
      return CastStyles::Static;
    }
    return CastStyles::Static;
  }

private:
  clang::ASTContext &m_context;

  bool isNumLike(clang::QualType type) const {
    return type->isArithmeticType() || type->isEnumeralType();
  }

  bool isPointerIntegerMix(clang::QualType from, clang::QualType to) const {
    return (from->isPointerType() && to->isIntegerType()) ||
           (from->isIntegerType() && to->isPointerType());
  }

  bool isOpaquePointerConversion(clang::QualType from,
                                 clang::QualType to) const {
    if (!from->isPointerType() || !to->isPointerType()) {
      return false;
    }
    const clang::QualType fromPointee = from->getPointeeType();
    const clang::QualType toPointee = to->getPointeeType();
    return fromPointee->isVoidType() || toPointee->isVoidType();
  }

  bool isUnrelatedPointerConversion(clang::QualType from,
                                    clang::QualType to) const {
    if (!from->isPointerType() || !to->isPointerType()) {
      return false;
    }
    const clang::QualType fromPointee =
        from->getPointeeType().getCanonicalType();
    const clang::QualType toPointee = to->getPointeeType().getCanonicalType();

    if (fromPointee.getUnqualifiedType() == toPointee.getUnqualifiedType()) {
      return false;
    }
    if (areRelatedRecords(fromPointee, toPointee)) {
      return false;
    }
    if (fromPointee->isVoidType() || toPointee->isVoidType()) {
      return false;
    }

    return true;
  }

  bool isCvAdjustment(clang::QualType from, clang::QualType to) const {
    if (from->isPointerType() && to->isPointerType()) {
      const clang::QualType fromPointee =
          from->getPointeeType().getCanonicalType();
      const clang::QualType toPointee = to->getPointeeType().getCanonicalType();

      return fromPointee.getUnqualifiedType() ==
                 toPointee.getUnqualifiedType() &&
             fromPointee != toPointee;
    }

    if (to->isReferenceType()) {
      const clang::QualType fromBase =
          from.getNonReferenceType().getCanonicalType();
      const clang::QualType toBase =
          to.getNonReferenceType().getCanonicalType();

      return fromBase.getUnqualifiedType() == toBase.getUnqualifiedType() &&
             fromBase != toBase;
    }

    return false;
  }

  bool areRelatedRecords(clang::QualType lhs, clang::QualType rhs) const {
    const auto *lhsDecl = lhs->getAsCXXRecordDecl();
    const auto *rhsDecl = rhs->getAsCXXRecordDecl();
    if (!lhsDecl || !rhsDecl) {
      return false;
    }
    lhsDecl = lhsDecl->getDefinition();
    rhsDecl = rhsDecl->getDefinition();
    if (!lhsDecl || !rhsDecl) {
      return false;
    }
    return lhsDecl == rhsDecl || lhsDecl->isDerivedFrom(rhsDecl) ||
           rhsDecl->isDerivedFrom(lhsDecl);
  }

  bool isPolymorphicDowncast(clang::QualType from, clang::QualType to) const {
    clang::QualType fromBase = from;
    clang::QualType toBase = to;

    if (fromBase->isPointerType() && toBase->isPointerType()) {
      fromBase = fromBase->getPointeeType();
      toBase = toBase->getPointeeType();
    } else if (fromBase->isReferenceType() && toBase->isReferenceType()) {
      fromBase = fromBase->getPointeeType();
      toBase = toBase->getPointeeType();
    } else {
      return false;
    }

    const auto *fromDecl = fromBase->getAsCXXRecordDecl();
    const auto *toDecl = toBase->getAsCXXRecordDecl();
    if (!fromDecl || !toDecl) {
      return false;
    }
    fromDecl = fromDecl->getDefinition();
    toDecl = toDecl->getDefinition();
    if (!fromDecl || !toDecl) {
      return false;
    }
    if (!fromDecl->isPolymorphic()) {
      return false;
    }
    return toDecl->isDerivedFrom(fromDecl);
  }
};

class SourceTextBuilder {
public:
  explicit SourceTextBuilder(clang::ASTContext &context) : m_context(context) {}

  std::string doReplace(const clang::CStyleCastExpr *cast,
                        CastStyles style) const {
    const std::string typeText =
        getText(cast->getTypeInfoAsWritten()->getTypeLoc().getSourceRange());
    const std::string exprText = getText(cast->getSubExpr()->getSourceRange());

    if (typeText.empty() || exprText.empty()) {
      return {};
    }

    return getCastStyleName(style).str() + "<" + typeText + ">(" + exprText +
           ")";
  }

private:
  clang::ASTContext &m_context;

  std::string getText(clang::SourceRange range) const {
    if (range.isInvalid()) {
      return {};
    }
    const clang::SourceManager &sm = m_context.getSourceManager();
    const clang::LangOptions &lang = m_context.getLangOpts();

    return clang::Lexer::getSourceText(
               clang::CharSourceRange::getTokenRange(range), sm, lang)
        .str();
  }
};

class RewriteOldStyleCastVisitor final
    : public clang::RecursiveASTVisitor<RewriteOldStyleCastVisitor> {
public:
  RewriteOldStyleCastVisitor(clang::ASTContext &context,
                             clang::Rewriter &rewriter)
      : m_context(context), m_rewriter(rewriter), m_classifier(context),
        m_textBuilder(context) {}

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *cast) {
    if (!cast) {
      return true;
    }
    if (!shouldProcess(cast)) {
      return true;
    }
    const CastStyles style = m_classifier.chooseCast(cast);
    const std::string repl = m_textBuilder.doReplace(cast, style);

    if (repl.empty()) {
      return true;
    }
    const clang::CharSourceRange fullRange =
        clang::CharSourceRange::getTokenRange(cast->getSourceRange());
    m_rewriter.ReplaceText(fullRange, repl);
    return true;
  }

private:
  clang::ASTContext &m_context;
  clang::Rewriter &m_rewriter;
  TypeCastClassifier m_classifier;
  SourceTextBuilder m_textBuilder;

  bool shouldProcess(const clang::CStyleCastExpr *cast) const {
    clang::SourceManager &sm = m_context.getSourceManager();
    const clang::SourceLocation loc = cast->getBeginLoc();

    if (loc.isInvalid()) {
      return false;
    }
    if (loc.isMacroID()) {
      return false;
    }
    if (sm.isInSystemHeader(loc)) {
      return false;
    }
    return sm.isWrittenInMainFile(sm.getSpellingLoc(loc));
  }
};

class RewriteOldStyleCastConsumer final : public clang::ASTConsumer {
public:
  RewriteOldStyleCastConsumer(clang::ASTContext &context,
                              clang::Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  RewriteOldStyleCastVisitor m_visitor;
};

class RewriteOldStyleCastAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<RewriteOldStyleCastConsumer>(ci.getASTContext(),
                                                         m_rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    clang::SourceManager &sm = m_rewriter.getSourceMgr();
    m_rewriter.getEditBuffer(sm.getMainFileID()).write(llvm::outs());
  }

private:
  clang::Rewriter m_rewriter;
};

} // namespace

static clang::FrontendPluginRegistry::Add<RewriteOldStyleCastAction>
    X("cast_rewriter", "Rewrite C-style casts into C++ casts");