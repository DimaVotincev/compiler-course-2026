#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

class CastReplaceVisitor : public RecursiveASTVisitor<CastReplaceVisitor> {
public:
  CastReplaceVisitor(ASTContext *Context, Rewriter &R)
      : Context(Context), Rewrite(R) {}

  bool VisitCStyleCastExpr(CStyleCastExpr *Cast) {
    SourceManager &SM = Context->getSourceManager();
    const Expr *SubExpr = Cast->getSubExpr();

    std::string CastType = "static_cast";
    CastKind Kind = Cast->getCastKind();

    // 1. BitCast и конверсия указателей в числа — это reinterpret_cast
    if (Kind == CK_BitCast || Kind == CK_PointerToIntegral ||
        Kind == CK_IntegralToPointer) {
      CastType = "reinterpret_cast";

      // 2. NoOp касты часто скрывают добавление/снятие const
    } else if (Kind == CK_NoOp) {
      QualType DestT = Cast->getType();
      QualType SrcT = SubExpr->getType();

      // Извлекаем "внутренний" тип, если это указатели
      if (DestT->isPointerType() && SrcT->isPointerType()) {
        QualType DestPointee = DestT->getPointeeType();
        QualType SrcPointee = SrcT->getPointeeType();
        if (DestPointee.isConstQualified() != SrcPointee.isConstQualified() ||
            DestPointee.isVolatileQualified() !=
                SrcPointee.isVolatileQualified()) {
          CastType = "const_cast";
        }
      }
      // И на всякий случай проверяем ссылки
      else if (DestT->isReferenceType() && SrcT->isReferenceType()) {
        QualType DestPointee = DestT->getPointeeType();
        QualType SrcPointee = SrcT->getPointeeType();
        if (DestPointee.isConstQualified() != SrcPointee.isConstQualified() ||
            DestPointee.isVolatileQualified() !=
                SrcPointee.isVolatileQualified()) {
          CastType = "const_cast";
        }
      }
    }

    std::string DestTypeStr = Cast->getTypeAsWritten().getAsString();
    std::string Replacement = CastType + "<" + DestTypeStr + ">(";

    // Заменяем `(Type)` на `cxx_cast<Type>(`
    SourceRange CastRange(Cast->getLParenLoc(), Cast->getRParenLoc());
    Rewrite.ReplaceText(CastRange, Replacement);

    // Получаем честный конец выражения и вставляем закрывающую скобку
    // Используем InsertText вместо InsertTextAfterToken, чтобы избежать
    // двойного сдвига
    SourceLocation EndLoc = Lexer::getLocForEndOfToken(
        SubExpr->getEndLoc(), 0, SM, Context->getLangOpts());
    Rewrite.InsertText(EndLoc, ")");

    return true;
  }

private:
  ASTContext *Context;
  Rewriter &Rewrite;
};

class CastReplaceConsumer final : public ASTConsumer {
public:
  CastReplaceConsumer(ASTContext *Context, Rewriter &R) : Visitor(Context, R) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  CastReplaceVisitor Visitor;
};

class CastReplaceAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<CastReplaceConsumer>(&CI.getASTContext(),
                                                 TheRewriter);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }

  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
  }

private:
  Rewriter TheRewriter;
};

} // namespace

static FrontendPluginRegistry::Add<CastReplaceAction>
    X("cast_replace_plugin", "Replaces C-style casts with C++ casts");