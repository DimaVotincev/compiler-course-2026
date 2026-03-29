#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class SimpleMutationAnalyzer
    : public clang::RecursiveASTVisitor<SimpleMutationAnalyzer> {
public:
  SimpleMutationAnalyzer(const clang::VarDecl *Target)
      : Target(Target), IsMutated(false) {}

  bool hasMutation() const { return IsMutated; }

  // Ловим присваивания: =, +=, -= и т.д.
  bool VisitBinaryOperator(clang::BinaryOperator *BinOp) {
    if (IsMutated)
      return true;
    if (BinOp->isAssignmentOp() || BinOp->isCompoundAssignmentOp()) {
      if (isTargetExpr(BinOp->getLHS())) {
        IsMutated = true;
      }
    }
    return true;
  }

  // Ловим инкременты/декременты: ++, --
  bool VisitUnaryOperator(clang::UnaryOperator *UnOp) {
    if (IsMutated)
      return true;
    if (UnOp->isIncrementDecrementOp()) {
      if (isTargetExpr(UnOp->getSubExpr())) {
        IsMutated = true;
      }
    }
    return true;
  }

private:
  const clang::VarDecl *Target;
  bool IsMutated;

  bool isTargetExpr(clang::Expr *E) {
    if (!E)
      return false;
    E = E->IgnoreParenImpCasts();

    // Прямое обращение
    if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
      return DRE->getDecl() == Target;
    }
    // Разыменование: *p
    if (auto *UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
      if (UO->getOpcode() == clang::UO_Deref) {
        return isTargetExpr(UO->getSubExpr());
      }
    }
    // Доступ по индексу массива: p[i]
    if (auto *ASE = llvm::dyn_cast<clang::ArraySubscriptExpr>(E)) {
      return isTargetExpr(ASE->getBase());
    }
    return false;
  }
};

class AddConstLocalVisitor
    : public clang::RecursiveASTVisitor<AddConstLocalVisitor> {
public:
  AddConstLocalVisitor(clang::FunctionDecl *funcDecl, clang::Rewriter &rewriter)
      : m_func(funcDecl), m_rewriter(rewriter) {}

  bool VisitVarDecl(clang::VarDecl *var) {
    if (!var->getTypeSourceInfo() || var->isImplicit())
      return true;

    SimpleMutationAnalyzer analyzer(var);
    if (m_func->getBody()) {
      analyzer.TraverseStmt(m_func->getBody());
    }

    if (analyzer.hasMutation())
      return true;

    clang::QualType type = var->getType();

    if (type->isReferenceType() &&
        !type.getNonReferenceType().isConstQualified()) {
      clang::SourceLocation loc = var->getTypeSpecStartLoc();
      if (loc.isValid() && loc.isFileID()) {
        m_rewriter.InsertTextBefore(loc, "const ");
      }
    } else if (type->isPointerType()) {
      clang::QualType pointeeType = type->getPointeeType();
      if (!pointeeType.isConstQualified()) {
        clang::SourceLocation loc = var->getTypeSpecStartLoc();
        if (loc.isValid() && loc.isFileID()) {
          m_rewriter.InsertTextBefore(loc, "const ");
          m_rewriter.InsertTextBefore(var->getLocation(), "const ");
        }
      }
    }
    return true;
  }

private:
  clang::FunctionDecl *m_func;
  clang::Rewriter &m_rewriter;
};

class AddConstVisitor final
    : public clang::RecursiveASTVisitor<AddConstVisitor> {
public:
  explicit AddConstVisitor(clang::ASTContext *context,
                           clang::Rewriter &rewriter)
      : m_context(context), m_rewriter(rewriter) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (!func->doesThisDeclarationHaveABody())
      return true;

    AddConstLocalVisitor localVisitor(func, m_rewriter);
    localVisitor.TraverseDecl(func);

    return true;
  }

private:
  clang::ASTContext *m_context;
  clang::Rewriter &m_rewriter;
};

class AddConstConsumer final : public clang::ASTConsumer {
public:
  explicit AddConstConsumer(clang::ASTContext *context,
                            clang::Rewriter &rewriter)
      : m_visitor(context, rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  AddConstVisitor m_visitor;
};

class AddConstAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<AddConstConsumer>(&ci.getASTContext(), m_rewriter);
  }

  void EndSourceFileAction() override {
    m_rewriter.getEditBuffer(m_rewriter.getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

private:
  clang::Rewriter m_rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<AddConstAction>
    X("sakharov_add_const",
      "Replaces non-const references and pointers with const");
