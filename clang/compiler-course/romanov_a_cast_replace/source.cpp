#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {

static std::string getCppCastWord(const clang::CStyleCastExpr *expr) {
  switch (expr->getCastKind()) {

  case clang::CK_BitCast:
  case clang::CK_LValueBitCast:
  case clang::CK_LValueToRValueBitCast: {
    clang::QualType src = expr->getSubExpr()->getType();
    clang::QualType dst = expr->getType();

    // void* → T* and T* → void* will be replaced with static_cast
    if ((src->isPointerType() && src->getPointeeType()->isVoidType()) ||
        (dst->isPointerType() && dst->getPointeeType()->isVoidType())) {
      return "static_cast";
    }
    return "reinterpret_cast";
  }

  case clang::CK_ReinterpretMemberPointer:
  case clang::CK_IntegralToPointer:
  case clang::CK_PointerToIntegral:
    return "reinterpret_cast";

  case clang::CK_NoOp: {
    clang::QualType src = expr->getSubExpr()->getType();
    clang::QualType dst = expr->getType();

    if (src->isPointerType() && dst->isPointerType()) {
      src = src->getPointeeType();
      dst = dst->getPointeeType();
    }

    if (src.isConstQualified() != dst.isConstQualified() ||
        src.isVolatileQualified() != dst.isVolatileQualified()) {
      return "const_cast";
    }
    return "static_cast";
  }

  default:
    return "static_cast";
  }
}

class RomanovACastReplaceVisitor final
    : public clang::RecursiveASTVisitor<RomanovACastReplaceVisitor> {
public:
  RomanovACastReplaceVisitor(clang::ASTContext *context,
                             clang::Rewriter &rewriter)
      : m_context(context), m_rewriter(rewriter) {}

  bool shouldTraversePostOrder() const { return true; }

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *expr) {
    std::string cast_keyword = getCppCastWord(expr);
    std::string cast_type_keyword =
        expr->getTypeAsWritten().getAsString(m_context->getPrintingPolicy());

    clang::Expr *sub_expr = expr->getSubExprAsWritten();
    std::string sub_expr_text = m_rewriter.getRewrittenText(
        clang::CharSourceRange::getTokenRange(sub_expr->getSourceRange()));

    std::string replacement_cast =
        cast_keyword + "<" + cast_type_keyword + ">(" + sub_expr_text + ")";

    m_rewriter.ReplaceText(
        clang::CharSourceRange::getTokenRange(expr->getSourceRange()),
        replacement_cast);

    return true;
  }

private:
  clang::ASTContext *m_context;
  clang::Rewriter &m_rewriter;
};

class RomanovACastReplaceConsumer final : public clang::ASTConsumer {
public:
  RomanovACastReplaceConsumer(clang::ASTContext *context,
                              clang::Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  RomanovACastReplaceVisitor m_visitor;
};

class RomanovACastReplaceAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<RomanovACastReplaceConsumer>(&ci.getASTContext(),
                                                         m_rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  void EndSourceFileAction() override {
    m_rewriter.getEditBuffer(m_rewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

private:
  clang::Rewriter m_rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<RomanovACastReplaceAction>
    X("romanov_a_cast_replace_plugin",
      "Plugin for replacing C-style casts with C++-style casts");
