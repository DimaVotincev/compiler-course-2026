#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class CastStyleAnalyzer {
public:
  static llvm::StringRef deduceCppCast(const clang::CStyleCastExpr *CastExpr) {
    const clang::CastKind Kind = CastExpr->getCastKind();
    const clang::QualType DstType = CastExpr->getType();
    const clang::QualType SrcType = CastExpr->getSubExpr()->getType();

    if (requiresDynamicCast(DstType, SrcType)) {
      return "dynamic_cast";
    }

    if (requiresConstCast(Kind, DstType, SrcType)) {
      return "const_cast";
    }

    if (requiresReinterpretCast(Kind, DstType, SrcType)) {
      return "reinterpret_cast";
    }

    return "static_cast";
  }

private:
  static bool requiresDynamicCast(clang::QualType DstType,
                                  clang::QualType SrcType) {
    if (!DstType->isPointerType() && !DstType->isReferenceType())
      return false;
    if (!SrcType->isPointerType() && !SrcType->isReferenceType())
      return false;

    const clang::QualType DstPointee = DstType->getPointeeType();
    const clang::QualType SrcPointee = SrcType->getPointeeType();

    const clang::CXXRecordDecl *DstDecl = DstPointee->getAsCXXRecordDecl();
    const clang::CXXRecordDecl *SrcDecl = SrcPointee->getAsCXXRecordDecl();

    if (!DstDecl || !SrcDecl)
      return false;

    if (!SrcDecl->isPolymorphic())
      return false;

    return DstDecl->isDerivedFrom(SrcDecl) || SrcDecl->isDerivedFrom(DstDecl);
  }

  static bool requiresConstCast(clang::CastKind Kind, clang::QualType DstType,
                                clang::QualType SrcType) {
    if (Kind != clang::CK_NoOp)
      return false;

    if (DstType->isPointerType() && SrcType->isPointerType()) {
      const clang::QualType DstPointee = DstType->getPointeeType();
      const clang::QualType SrcPointee = SrcType->getPointeeType();

      return (DstPointee.getCVRQualifiers() != SrcPointee.getCVRQualifiers()) &&
             (DstPointee.getTypePtr() == SrcPointee.getTypePtr());
    }

    return false;
  }

  static bool requiresReinterpretCast(clang::CastKind Kind,
                                      clang::QualType DstType,
                                      clang::QualType SrcType) {
    if (Kind == clang::CK_BitCast) {
      if (DstType->isVoidPointerType() || SrcType->isVoidPointerType())
        return false;

      if (DstType->isPointerType() && SrcType->isPointerType()) {
        const clang::QualType DstUnqual =
            DstType->getPointeeType().getUnqualifiedType();
        const clang::QualType SrcUnqual =
            SrcType->getPointeeType().getUnqualifiedType();
        return DstUnqual.getTypePtr() != SrcUnqual.getTypePtr();
      }
      return true;
    }

    return Kind == clang::CK_PointerToIntegral ||
           Kind == clang::CK_IntegralToPointer ||
           Kind == clang::CK_ReinterpretMemberPointer;
  }
};

class CastModernizerVisitor final
    : public clang::RecursiveASTVisitor<CastModernizerVisitor> {
public:
  explicit CastModernizerVisitor(clang::ASTContext &Ctx, clang::Rewriter &R)
      : Context(Ctx), SourceRewriter(R) {}

  bool shouldTraversePostOrder() const { return true; }

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *CastExpr) {
    if (CastExpr->getBeginLoc().isMacroID())
      return true;

    llvm::StringRef CastKeyword = CastStyleAnalyzer::deduceCppCast(CastExpr);

    std::string DstTypeStr =
        CastExpr->getTypeAsWritten().getAsString(Context.getPrintingPolicy());

    const clang::Expr *SubExpr = CastExpr->getSubExprAsWritten();
    clang::CharSourceRange SubExprRange =
        clang::CharSourceRange::getTokenRange(SubExpr->getSourceRange());
    std::string SubExprStr = SourceRewriter.getRewrittenText(SubExprRange);

    std::string Replacement =
        CastKeyword.str() + "<" + DstTypeStr + ">(" + SubExprStr + ")";

    clang::CharSourceRange FullCastRange =
        clang::CharSourceRange::getTokenRange(CastExpr->getSourceRange());
    SourceRewriter.ReplaceText(FullCastRange, Replacement);

    return true;
  }

private:
  clang::ASTContext &Context;
  clang::Rewriter &SourceRewriter;
};

class CastModernizerConsumer final : public clang::ASTConsumer {
public:
  explicit CastModernizerConsumer(clang::ASTContext &Ctx, clang::Rewriter &R)
      : Visitor(Ctx, R) {}

  void HandleTranslationUnit(clang::ASTContext &Ctx) override {
    Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
  }

private:
  CastModernizerVisitor Visitor;
};

class CastModernizerAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<CastModernizerConsumer>(CI.getASTContext(),
                                                    TheRewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    clang::FileID MainFileID = TheRewriter.getSourceMgr().getMainFileID();
    TheRewriter.getEditBuffer(MainFileID).write(llvm::outs());
  }

private:
  clang::Rewriter TheRewriter;
};

} // namespace

static clang::FrontendPluginRegistry::Add<CastModernizerAction>
    X("shekhirev_v_cast_replace_plugin",
      "Plugin for replacing C-style casts with C++-style casts");