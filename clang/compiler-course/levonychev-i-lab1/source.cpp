#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

namespace {

class MutationFinder : public RecursiveASTVisitor<MutationFinder> {
  const VarDecl *Target;
  bool VarMutated = false;
  bool DataMutated = false;

public:
  explicit MutationFinder(const VarDecl *D) : Target(D) {}

  bool isVarMutated() const { return VarMutated; }
  bool isDataMutated() const { return DataMutated; }
  bool VisitBinaryOperator(BinaryOperator *Node) {
    if (!Node->isAssignmentOp())
      return true;

    Expr *LHS = Node->getLHS()->IgnoreParenImpCasts();

    if (auto *DRE = dyn_cast<DeclRefExpr>(LHS)) {
      if (DRE->getDecl() == Target) {
        VarMutated = true;
      }
    }

    if (auto *UO = dyn_cast<UnaryOperator>(LHS)) {
      if (UO->getOpcode() == UO_Deref) {
        if (auto *DRE = dyn_cast<DeclRefExpr>(
                UO->getSubExpr()->IgnoreParenImpCasts())) {
          if (DRE->getDecl() == Target) {
            DataMutated = true;
          }
        }
      }
    }
    return true;
  }

  bool VisitUnaryOperator(UnaryOperator *Node) {
    if (!Node->isIncrementDecrementOp())
      return true;

    Expr *Sub = Node->getSubExpr()->IgnoreParenImpCasts();

    if (auto *DRE = dyn_cast<DeclRefExpr>(Sub)) {
      if (DRE->getDecl() == Target)
        VarMutated = true;
    }

    if (auto *UO = dyn_cast<UnaryOperator>(Sub)) {
      if (UO->getOpcode() == UO_Deref) {
        if (auto *DRE = dyn_cast<DeclRefExpr>(
                UO->getSubExpr()->IgnoreParenImpCasts())) {
          if (DRE->getDecl() == Target)
            DataMutated = true;
        }
      }
    }
    return true;
  }
};

class ConstAnalysisVisitor : public RecursiveASTVisitor<ConstAnalysisVisitor> {
  Rewriter &TheRewriter;

public:
  explicit ConstAnalysisVisitor(Rewriter &R) : TheRewriter(R) {}
  bool VisitVarDecl(VarDecl *D) {

    QualType T = D->getType();
    bool isPtr = T->isPointerType();
    bool isRef = T->isReferenceType();

    if (!isPtr && !isRef)
      return true;

    if (auto *FD = dyn_cast<FunctionDecl>(D->getDeclContext())) {
      if (Stmt *Body = FD->getBody()) {
        MutationFinder Finder(D);
        Finder.TraverseStmt(Body);
        SourceLocation StartLoc = D->getBeginLoc();
        SourceLocation VarNameLoc = D->getLocation();
        if (isPtr) {
          bool canBeDataConst = !T->getPointeeType().isConstQualified() &&
                                !Finder.isDataMutated();
          bool canBePtrConst = !T.isConstQualified() && !Finder.isVarMutated();

          if (canBeDataConst && canBePtrConst) {
            TheRewriter.InsertText(StartLoc, "const ", true, true);
            TheRewriter.InsertText(VarNameLoc, "const ");
          } else if (canBeDataConst) {
            TheRewriter.InsertText(StartLoc, "const ", true, true);
          } else if (canBePtrConst) {
            TheRewriter.InsertText(VarNameLoc, "const ");
          }
        } else if (isRef) {
          if (!T.getNonReferenceType().isConstQualified() &&
              !Finder.isVarMutated()) {
            TheRewriter.InsertText(StartLoc, "const ", true, true);
          }
        }
      }
    }
    return true;
  }
};

class ConstAnalysisConsumer : public ASTConsumer {
  Rewriter TheRewriter;

public:
  void HandleTranslationUnit(ASTContext &Context) override {
    TheRewriter.setSourceMgr(Context.getSourceManager(), Context.getLangOpts());
    ConstAnalysisVisitor Visitor(TheRewriter);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    const llvm::RewriteBuffer *RewriteBuf = TheRewriter.getRewriteBufferFor(
        Context.getSourceManager().getMainFileID());
    if (RewriteBuf) {
      llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    }
  }
};

class ConstAnalysisAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef) override {
    return std::make_unique<ConstAnalysisConsumer>();
  }
  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ConstAnalysisAction>
    X("const_change_plugin",
      "Suggests const for pointers and references based on mutations");