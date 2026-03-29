#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

class CStyleCastVisitor : public RecursiveASTVisitor<CStyleCastVisitor> {
public:
  CStyleCastVisitor(ASTContext *Ctx, Rewriter &R) : Context(Ctx), RW(R) {}

  bool VisitCStyleCastExpr(CStyleCastExpr *Node) {
    SourceManager &SM = Context->getSourceManager();

    if (SM.isInSystemHeader(Node->getBeginLoc()))
      return true;

    const Expr *SubExpr = Node->getSubExpr();

    std::string CastName = determineCastKind(Node, SubExpr);
    std::string DestType = Node->getTypeAsWritten().getAsString();

    CharSourceRange ExprRange =
        CharSourceRange::getTokenRange(SubExpr->getSourceRange());

    std::string ExprText =
        Lexer::getSourceText(ExprRange, SM, Context->getLangOpts()).str();

    std::string Replacement = CastName + "<" + DestType + ">(" + ExprText + ")";

    CharSourceRange FullRange =
        CharSourceRange::getTokenRange(Node->getSourceRange());

    RW.ReplaceText(FullRange, Replacement);

    return true;
  }

private:
  std::string determineCastKind(CStyleCastExpr *Node, const Expr *SubExpr) {
    CastKind Kind = Node->getCastKind();

    QualType SrcType = SubExpr->getType();
    QualType DstType = Node->getType();

    if (SrcType->isPointerType() && DstType->isPointerType()) {
      QualType SrcPointee = SrcType->getPointeeType();
      QualType DstPointee = DstType->getPointeeType();

      if (SrcPointee.isConstQualified() != DstPointee.isConstQualified() ||
          SrcPointee.isVolatileQualified() !=
              DstPointee.isVolatileQualified()) {
        return "const_cast";
      }
    }

    if (Kind == CK_BitCast || Kind == CK_LValueBitCast ||
        Kind == CK_PointerToIntegral || Kind == CK_IntegralToPointer) {
      return "reinterpret_cast";
    }

    return "static_cast";
  }

  ASTContext *Context;
  Rewriter &RW;
};

class CStyleCastConsumer : public ASTConsumer {
public:
  CStyleCastConsumer(ASTContext *Ctx, Rewriter &R) : Visitor(Ctx, R) {}

  void HandleTranslationUnit(ASTContext &Ctx) override {
    Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
  }

private:
  CStyleCastVisitor Visitor;
};

class CStyleCastAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {

    RewriterInstance.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    return std::make_unique<CStyleCastConsumer>(&CI.getASTContext(),
                                                RewriterInstance);
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    SourceManager &SM = RewriterInstance.getSourceMgr();
    RewriterInstance.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
  }

private:
  Rewriter RewriterInstance;
};

} // namespace

static FrontendPluginRegistry::Add<CStyleCastAction>
    X("replace_c_cast", "Replace C-style casts with C++ casts");