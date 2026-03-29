#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <set>

using namespace clang;

namespace {

class MutationAnalyzer : public RecursiveASTVisitor<MutationAnalyzer> {
public:
  bool VisitBinaryOperator(BinaryOperator *BO) {
    if (BO->isAssignmentOp()) {
      AnalyzeLValue(BO->getLHS(), false);
    }
    return true;
  }

  bool VisitUnaryOperator(UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp()) {
      AnalyzeLValue(UO->getSubExpr(), false);
    }
    return true;
  }

  bool isPointerMutated(const VarDecl *VD) const {
    return MutatedPointers.count(VD) > 0;
  }

  bool isPointeeMutated(const VarDecl *VD) const {
    return MutatedPointees.count(VD) > 0;
  }

private:
  std::set<const VarDecl *> MutatedPointers;
  std::set<const VarDecl *> MutatedPointees;

  void AnalyzeLValue(Expr *E, bool isDeref) {
    if (!E)
      return;
    E = E->IgnoreParenCasts();

    if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        if (isDeref)
          MutatedPointees.insert(VD);
        else
          MutatedPointers.insert(VD);
      }
      return;
    }

    if (auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() == UO_Deref) {
        AnalyzeLValue(UO->getSubExpr(), true);
      }
      return;
    }

    if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      AnalyzeLValue(ASE->getBase(), true);
      return;
    }

    if (auto *BO = dyn_cast<BinaryOperator>(E)) {
      AnalyzeLValue(BO->getLHS(), isDeref);
      AnalyzeLValue(BO->getRHS(), isDeref);
      return;
    }
  }
};

class ConstInsertVisitor : public RecursiveASTVisitor<ConstInsertVisitor> {
public:
  ConstInsertVisitor(Rewriter &R, const MutationAnalyzer &Analyzer)
      : TheRewriter(R), TheAnalyzer(Analyzer) {}

  bool VisitVarDecl(VarDecl *VD) {
    QualType QT = VD->getType();
    if (QT.isNull())
      return true;

    bool isPtr = QT->isPointerType();
    bool isRef = QT->isReferenceType();

    if (!isPtr && !isRef)
      return true;

    if (!VD->isLocalVarDecl() && !isa<ParmVarDecl>(VD))
      return true;

    bool ptrMutated = TheAnalyzer.isPointerMutated(VD);
    bool dataMutated = TheAnalyzer.isPointeeMutated(VD);

    bool dataIsConst = QT->getPointeeType().isConstQualified();
    bool ptrIsConst = QT.isLocalConstQualified();

    if (isRef) {
      if (!dataMutated && !QT.getNonReferenceType().isConstQualified()) {
        TheRewriter.InsertTextBefore(VD->getBeginLoc(), "const ");
      }
    } else if (isPtr) {
      if (!ptrMutated && !dataMutated) {
        if (!dataIsConst)
          TheRewriter.InsertTextBefore(VD->getBeginLoc(), "const ");
        if (!ptrIsConst)
          TheRewriter.InsertTextBefore(VD->getLocation(), "const ");
      } else if (!dataMutated && ptrMutated) {
        if (!dataIsConst)
          TheRewriter.InsertTextBefore(VD->getBeginLoc(), "const ");
      } else if (dataMutated && !ptrMutated) {
        if (!ptrIsConst)
          TheRewriter.InsertTextBefore(VD->getLocation(), "const ");
      }
    }

    return true;
  }

private:
  Rewriter &TheRewriter;
  const MutationAnalyzer &TheAnalyzer;
};

class ConstQualifierConsumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Context) override {
    Rewriter TheRewriter;
    TheRewriter.setSourceMgr(Context.getSourceManager(), Context.getLangOpts());

    MutationAnalyzer Analyzer;
    Analyzer.TraverseDecl(Context.getTranslationUnitDecl());

    ConstInsertVisitor Visitor(TheRewriter, Analyzer);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());

    FileID MainFileID = Context.getSourceManager().getMainFileID();
    const auto *RewriteBuf = TheRewriter.getRewriteBufferFor(MainFileID);

    if (RewriteBuf) {
      llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    } else {
      bool Invalid = false;
      StringRef Buf =
          Context.getSourceManager().getBufferData(MainFileID, &Invalid);
      if (!Invalid) {
        llvm::outs() << Buf;
      }
    }
  }
};

class ConstQualifierAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<ConstQualifierConsumer>();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ConstQualifierAction>
    X("auto_const_plugin", "Adds const to unmodified variables");