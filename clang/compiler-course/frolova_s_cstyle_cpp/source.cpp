#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class CastRewriterVisitor final
    : public clang::RecursiveASTVisitor<CastRewriterVisitor> {
public:
  explicit CastRewriterVisitor(clang::ASTContext &C, clang::Rewriter &R)
      : Context(C), Rewrite(R) {}

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *Node) {
    clang::SourceManager &SM = Context.getSourceManager();

    if (!SM.isInMainFile(Node->getBeginLoc()) ||
        Node->getBeginLoc().isMacroID())
      return true;

    std::string CastName = "static_cast";
    clang::CastKind Kind = Node->getCastKind();

    if (Kind == clang::CK_BitCast || Kind == clang::CK_LValueBitCast ||
        Kind == clang::CK_PointerToIntegral ||
        Kind == clang::CK_IntegralToPointer ||
        Kind == clang::CK_ReinterpretMemberPointer) {
      CastName = "reinterpret_cast";
    } else if (Kind == clang::CK_NoOp) {
      clang::QualType SubType = Node->getSubExpr()->getType();
      clang::QualType TargetType = Node->getType();

      auto isConstCastCompatible = [&](clang::QualType From,
                                       clang::QualType To) -> bool {
        if (From->isReferenceType())
          From = From.getNonReferenceType();
        if (To->isReferenceType())
          To = To.getNonReferenceType();

        if (From->isPointerType() && To->isPointerType()) {
          clang::QualType FromPointee = From->getPointeeType();
          clang::QualType ToPointee = To->getPointeeType();
          return Context.hasSameUnqualifiedType(FromPointee, ToPointee) &&
                 (FromPointee.getCVRQualifiers() !=
                  ToPointee.getCVRQualifiers());
        }

        return Context.hasSameUnqualifiedType(From, To) &&
               (From.getCVRQualifiers() != To.getCVRQualifiers());
      };

      if (isConstCastCompatible(SubType, TargetType)) {
        CastName = "const_cast";
      }
    }

    std::string TypeStr = Node->getTypeAsWritten().getAsString();

    clang::SourceLocation SubExprLoc =
        Node->getSubExprAsWritten()->getBeginLoc();

    clang::SourceRange CastRange(Node->getBeginLoc(),
                                 SubExprLoc.getLocWithOffset(-1));
    std::string Replacement = CastName + "<" + TypeStr + ">(";
    Rewrite.ReplaceText(CastRange, Replacement);

    clang::SourceLocation EndAfterSubExpr = clang::Lexer::getLocForEndOfToken(
        Node->getSubExpr()->getEndLoc(), 0, SM, Context.getLangOpts());
    Rewrite.InsertTextAfter(EndAfterSubExpr, ")");

    return true;
  }

private:
  clang::ASTContext &Context;
  clang::Rewriter &Rewrite;
};

class CastConsumer final : public clang::ASTConsumer {
public:
  explicit CastConsumer(clang::CompilerInstance &CI) : CI(CI) {
    Rewrite.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
  }

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    CastRewriterVisitor Visitor(Context, Rewrite);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());

    Rewrite.getEditBuffer(CI.getSourceManager().getMainFileID())
        .write(llvm::outs());
  }

private:
  clang::CompilerInstance &CI;
  clang::Rewriter Rewrite;
};

class CastAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<CastConsumer>(CI);
  }

  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<CastAction>
    X("cstyle_cast_replacer",
      "Replace C-style casts with C++ casts and rewrite code");
