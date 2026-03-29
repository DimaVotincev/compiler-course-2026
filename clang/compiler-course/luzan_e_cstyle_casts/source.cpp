#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

namespace {
class LuzanECstyleCastsVisitor final
    : public clang::RecursiveASTVisitor<LuzanECstyleCastsVisitor> {
public:
  explicit LuzanECstyleCastsVisitor(clang::ASTContext *context,
                                    clang::Rewriter &rewriter_)
      : m_context(context), rewriter(rewriter_) {}

  /// for nested casts
  bool TraverseCStyleCastExpr(clang::CStyleCastExpr *expr) {
    TraverseStmt(expr->getSubExpr());
    VisitCStyleCastExpr(expr);
    return true;
  }

  bool VisitCStyleCastExpr(clang::CStyleCastExpr *expr) {
    /// determine which cast type is it
    auto castNameOpt = getCastName(expr->getCastKind());
    if (!castNameOpt) {
      return true;
    }
    std::string castName = isConstCast(expr) ? "const_cast" : *castNameOpt;
    /// get the target type of cast
    std::string targetType = expr->getTypeAsWritten().getAsString();
    /// get positions of token/text to rewrite
    clang::CharSourceRange range =
        clang::CharSourceRange::getTokenRange(expr->getSourceRange());

    /// get casting expression
    clang::Expr *subExpr = expr->getSubExpr();
    /// get range of casting expr / subexpr
    clang::CharSourceRange subRange =
        clang::CharSourceRange::getTokenRange(subExpr->getSourceRange());
    /// get rw subexpr text
    std::string subExprText = rewriter.getRewrittenText(subRange);

    /// cpp style cast
    std::string cpp_cast =
        castName + "<" + targetType + ">(" + subExprText + ")";

    rewriter.ReplaceText(range, cpp_cast);

    return true;
  }

private:
  clang::ASTContext *m_context;
  clang::Rewriter &rewriter;

  bool isConstCast(const clang::CStyleCastExpr *expr) {
    clang::QualType src = expr->getSubExpr()->getType();
    clang::QualType dst = expr->getType();

    if (src->isPointerType() && dst->isPointerType()) {
      src = src->getPointeeType();
      dst = dst->getPointeeType();
    }

    return src.isConstQualified() != dst.isConstQualified() ||
           src.isVolatileQualified() != dst.isVolatileQualified();
  }

  std::optional<std::string> getCastName(clang::CastKind kind) {

    switch (kind) {

    case clang::CK_BitCast:
    case clang::CK_LValueBitCast:
      return "reinterpret_cast";

    case clang::CK_NoOp:
    case clang::CK_IntegralCast:
    case clang::CK_FloatingCast:
    case clang::CK_IntegralToFloating:
    case clang::CK_FloatingToIntegral:
    case clang::CK_BaseToDerived:
    case clang::CK_DerivedToBase:
      return "static_cast";

    default:
      return std::nullopt;
    }
  }
};

class LuzanECstyleCastsConsumer final : public clang::ASTConsumer {
public:
  explicit LuzanECstyleCastsConsumer(clang::ASTContext *context,
                                     clang::Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  LuzanECstyleCastsVisitor m_visitor;
};

class LuzanECstyleCastsAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<LuzanECstyleCastsConsumer>(&ci.getASTContext(),
                                                       rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

  void EndSourceFileAction() override {
    rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

private:
  clang::Rewriter rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<LuzanECstyleCastsAction>
    X("luzan_e_cstyle_casts",
      "Plugin for replacing c-style casts with cpp-style casts");
