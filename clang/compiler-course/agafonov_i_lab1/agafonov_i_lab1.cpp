#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/RewriteBuffer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

namespace {

struct MutationState {
  bool PointerChanged = false;
  bool DataChanged = false;
};

class EnhancedMutationAnalyzer
    : public RecursiveASTVisitor<EnhancedMutationAnalyzer> {
  const VarDecl *Target;
  MutationState &State;

public:
  EnhancedMutationAnalyzer(const VarDecl *D, MutationState &S)
      : Target(D), State(S) {}

  bool VisitBinaryOperator(BinaryOperator *BO) {
    if (BO->isAssignmentOp() || BO->isCompoundAssignmentOp())
      analyzeLValue(BO->getLHS(), false);
    return true;
  }

  bool VisitUnaryOperator(UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp())
      analyzeLValue(UO->getSubExpr(), false);
    return true;
  }

private:
  void analyzeLValue(Expr *E, bool IsDeref) {
    if (!E)
      return;
    E = E->IgnoreParenImpCasts();
    if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      if (DRE->getDecl() == Target) {
        if (IsDeref)
          State.DataChanged = true;
        else
          State.PointerChanged = true;
      }
    } else if (auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() == UO_Deref)
        analyzeLValue(UO->getSubExpr(), true);
    } else if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      analyzeLValue(ASE->getBase(), true);
    }
  }
};

class ConstifyVisitor : public RecursiveASTVisitor<ConstifyVisitor> {
  Rewriter &TheRewriter;

public:
  explicit ConstifyVisitor(Rewriter &R) : TheRewriter(R) {}

  bool VisitVarDecl(VarDecl *D) {
    if (D->hasGlobalStorage() || D->isImplicit() ||
        D->getType().isConstQualified())
      return true;

    QualType T = D->getType();
    if (!T->isPointerType() && !T->isReferenceType())
      return true;

    Stmt *Body = nullptr;
    if (auto *FD = dyn_cast<FunctionDecl>(D->getDeclContext()))
      Body = FD->getBody();
    else if (auto *MD = dyn_cast<CXXMethodDecl>(D->getDeclContext()))
      Body = MD->getBody();

    if (Body) {
      MutationState MS;
      EnhancedMutationAnalyzer(D, MS).TraverseStmt(Body);
      applyFixes(D, MS);
    }
    return true;
  }

private:
  void applyFixes(VarDecl *D, const MutationState &MS) {
    QualType T = D->getType();
    SourceLocation BeginLoc = D->getTypeSpecStartLoc();

    if (!MS.DataChanged) {
      bool alreadyDataConst = T->isReferenceType()
                                  ? T.getNonReferenceType().isConstQualified()
                                  : T->getPointeeType().isConstQualified();
      if (!alreadyDataConst && BeginLoc.isValid())
        TheRewriter.InsertText(BeginLoc, "const ", true);
    }
    if (T->isPointerType() && !MS.PointerChanged &&
        !T.isLocalConstQualified()) {
      SourceLocation NameLoc = D->getLocation();
      if (NameLoc.isValid())
        TheRewriter.InsertText(NameLoc, "const ", true);
    }
  }
};

class ConstifyConsumer : public ASTConsumer {
  Rewriter TheRewriter;

public:
  explicit ConstifyConsumer(CompilerInstance &CI) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    TheRewriter.setSourceMgr(Context.getSourceManager(), Context.getLangOpts());
    ConstifyVisitor(TheRewriter).TraverseDecl(Context.getTranslationUnitDecl());
    FileID ID = Context.getSourceManager().getMainFileID();
    if (const llvm::RewriteBuffer *Buf = TheRewriter.getRewriteBufferFor(ID))
      llvm::outs() << std::string(Buf->begin(), Buf->end());
    else {
      bool Invalid = false;
      StringRef Data = Context.getSourceManager().getBufferData(ID, &Invalid);
      if (!Invalid)
        llvm::outs() << Data;
    }
  }
};

class ConstifyAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef) override {
    return std::make_unique<ConstifyConsumer>(CI);
  }
  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};
} // namespace

static FrontendPluginRegistry::Add<ConstifyAction> X("constify_plugin",
                                                     "Advanced Const Lab");