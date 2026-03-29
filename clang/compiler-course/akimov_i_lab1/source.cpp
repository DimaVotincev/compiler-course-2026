#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>

using namespace clang;

namespace {

struct MutationInfo {
  bool pointerMutated = false;
  bool pointeeMutated = false;
};

class MutationCollector : public RecursiveASTVisitor<MutationCollector> {
public:
  bool VisitBinaryOperator(BinaryOperator *BO) {
    if (BO->isAssignmentOp())
      inspectLValue(BO->getLHS(), false);
    return true;
  }

  bool VisitUnaryOperator(UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp())
      inspectLValue(UO->getSubExpr(), false);
    return true;
  }

  MutationInfo getInfo(const VarDecl *VD) const {
    auto it = mutations.find(VD);
    return it != mutations.end() ? it->second : MutationInfo{};
  }

private:
  std::unordered_map<const VarDecl *, MutationInfo> mutations;

  void inspectLValue(Expr *E, bool indirect) {
    if (!E)
      return;
    E = E->IgnoreParenCasts();

    if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        MutationInfo &info = mutations[VD];
        if (indirect)
          info.pointeeMutated = true;
        else
          info.pointerMutated = true;
      }
      return;
    }

    if (auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() == UO_Deref) {
        inspectLValue(UO->getSubExpr(), true);
      }
      return;
    }

    if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      inspectLValue(ASE->getBase(), true);
      return;
    }

    if (auto *ME = dyn_cast<MemberExpr>(E)) {
      inspectLValue(ME->getBase(), true);
      return;
    }
  }
};

class ConstInserter : public RecursiveASTVisitor<ConstInserter> {
public:
  ConstInserter(Rewriter &R, MutationCollector &C)
      : rewriter(R), collector(C) {}

  bool VisitVarDecl(VarDecl *VD) {
    if (!VD->isLocalVarDecl() && !isa<ParmVarDecl>(VD))
      return true;

    QualType QT = VD->getType();
    if (QT.isNull())
      return true;

    bool isPtr = QT->isPointerType();
    bool isRef = QT->isReferenceType();
    if (!isPtr && !isRef)
      return true;

    MutationInfo info = collector.getInfo(VD);
    QualType pointee = isRef ? QT->getPointeeType() : QT->getPointeeType();
    bool pointeeIsConst = pointee.isConstQualified();
    bool ptrIsConst = QT.isConstQualified();

    if (isRef) {
      if (!info.pointeeMutated && !pointeeIsConst)
        rewriter.InsertTextBefore(VD->getBeginLoc(), "const ");
    } else if (isPtr) {
      bool needConstPointee = !info.pointeeMutated && !pointeeIsConst;
      bool needConstPtr = !info.pointerMutated && !ptrIsConst;

      if (needConstPointee)
        rewriter.InsertTextBefore(VD->getBeginLoc(), "const ");
      if (needConstPtr)
        rewriter.InsertTextBefore(VD->getLocation(), "const ");
    }
    return true;
  }

private:
  Rewriter &rewriter;
  MutationCollector &collector;
};

class ConstifyConsumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Ctx) override {
    MutationCollector collector;
    collector.TraverseDecl(Ctx.getTranslationUnitDecl());

    Rewriter rewriter;
    rewriter.setSourceMgr(Ctx.getSourceManager(), Ctx.getLangOpts());

    ConstInserter inserter(rewriter, collector);
    inserter.TraverseDecl(Ctx.getTranslationUnitDecl());

    FileID mainID = Ctx.getSourceManager().getMainFileID();
    if (const auto *buf = rewriter.getRewriteBufferFor(mainID)) {
      llvm::outs() << std::string(buf->begin(), buf->end());
    } else {
      bool invalid = false;
      llvm::StringRef original =
          Ctx.getSourceManager().getBufferData(mainID, &invalid);
      if (!invalid)
        llvm::outs() << original;
    }
  }
};

class ConstifyAction : public PluginASTAction {
protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<ConstifyConsumer>();
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ConstifyAction>
    X("constify_plugin",
      "Automatically add const to unmodified pointers and references");
