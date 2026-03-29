#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct MutationState {
  bool SelfMutated = false;
  bool PointeeMutated = false;
};

class ConstQualifierVisitor
    : public clang::RecursiveASTVisitor<ConstQualifierVisitor> {
public:
  ConstQualifierVisitor(clang::ASTContext &Context, clang::Rewriter &Rewriter)
      : Context(Context), Rewriter(Rewriter) {}

  bool VisitVarDecl(clang::VarDecl *Decl) {
    if (!isSupportedDecl(Decl)) {
      return true;
    }

    if (canAddConst(Decl->getType())) {
      Candidates.insert(Decl);
    }
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *Op) {
    if (Op->isAssignmentOp()) {
      mark(Op->getLHS(), true, false);
    }
    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *Op) {
    if (Op->isIncrementDecrementOp()) {
      mark(Op->getSubExpr(), true, false);
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *Call) {
    const auto *Callee = Call->getDirectCallee();
    if (!Callee) {
      return true;
    }

    const unsigned ArgsCount = Call->getNumArgs();
    const unsigned ParamsCount = Callee->getNumParams();
    const unsigned Count = ArgsCount < ParamsCount ? ArgsCount : ParamsCount;

    for (unsigned I = 0; I < Count; ++I) {
      analyzeArgument(Call->getArg(I), Callee->getParamDecl(I)->getType());
    }
    return true;
  }

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *Call) {
    const auto *Method = Call->getMethodDecl();
    if (!Method || Method->isConst()) {
      return true;
    }

    clang::Expr *Object = ignoreWrappers(Call->getImplicitObjectArgument());
    if (!Object) {
      return true;
    }

    if (Object->getType()->isPointerType()) {
      mark(Object, false, true);
    } else {
      mark(Object, true, false);
    }
    return true;
  }

  void applyRewrites() {
    for (const auto *Decl : Candidates) {
      const MutationState State = States.lookup(Decl);
      std::string Replacement = buildReplacementType(Decl, State);
      if (Replacement.empty()) {
        continue;
      }

      clang::TypeSourceInfo *TypeInfo = Decl->getTypeSourceInfo();
      if (!TypeInfo) {
        continue;
      }

      const clang::SourceRange Range = TypeInfo->getTypeLoc().getSourceRange();
      if (Range.isInvalid()) {
        continue;
      }

      const char Last = Replacement.back();
      const bool IsIdentChar = (Last >= 'a' && Last <= 'z') ||
                               (Last >= 'A' && Last <= 'Z') ||
                               (Last >= '0' && Last <= '9') || Last == '_';
      if (IsIdentChar) {
        Replacement.push_back(' ');
      }

      Rewriter.ReplaceText(Range, Replacement);
    }
  }

private:
  clang::ASTContext &Context;
  clang::Rewriter &Rewriter;
  llvm::DenseSet<const clang::VarDecl *> Candidates;
  llvm::DenseMap<const clang::VarDecl *, MutationState> States;

  clang::Expr *ignoreWrappers(clang::Expr *Expr) const {
    return Expr ? Expr->IgnoreParenImpCasts() : nullptr;
  }

  bool isSupportedDecl(const clang::VarDecl *Decl) const {
    if (!Decl) {
      return false;
    }

    const auto *Function =
        llvm::dyn_cast<clang::FunctionDecl>(Decl->getDeclContext());
    if (!Function || !Function->hasBody()) {
      return false;
    }

    if (!llvm::isa<clang::ParmVarDecl>(Decl) && !Decl->isLocalVarDecl()) {
      return false;
    }

    const clang::SourceManager &SM = Context.getSourceManager();
    if (!SM.isWrittenInMainFile(SM.getSpellingLoc(Decl->getLocation()))) {
      return false;
    }

    return Decl->getType()->isLValueReferenceType() ||
           Decl->getType()->isPointerType();
  }

  static bool canAddConst(clang::QualType Type) {
    if (Type->isLValueReferenceType()) {
      const clang::QualType Referenced = Type.getNonReferenceType();
      if (Referenced->isFunctionType()) {
        return false;
      }
      return !Referenced.isConstQualified();
    }

    if (Type->isPointerType()) {
      const clang::QualType Pointee = Type->getPointeeType();
      if (Pointee->isFunctionType()) {
        return false;
      }
      return !Type.isLocalConstQualified() || !Pointee.isConstQualified();
    }

    return false;
  }

  void markState(const clang::VarDecl *Decl, bool MarkSelf, bool MarkPointee) {
    if (!Decl || !Candidates.count(Decl)) {
      return;
    }

    MutationState &State = States[Decl];
    if (Decl->getType()->isLValueReferenceType()) {
      State.PointeeMutated |= MarkSelf || MarkPointee;
      return;
    }

    State.SelfMutated |= MarkSelf;
    State.PointeeMutated |= MarkPointee;
  }

  void mark(clang::Expr *Expr, bool MarkSelf, bool MarkPointee) {
    Expr = ignoreWrappers(Expr);
    if (!Expr) {
      return;
    }

    if (auto *Ref = llvm::dyn_cast<clang::DeclRefExpr>(Expr)) {
      if (const auto *Decl = llvm::dyn_cast<clang::VarDecl>(Ref->getDecl())) {
        markState(Decl, MarkSelf, MarkPointee);
      }
      return;
    }

    if (auto *Op = llvm::dyn_cast<clang::UnaryOperator>(Expr)) {
      switch (Op->getOpcode()) {
      case clang::UO_Deref:
        mark(Op->getSubExpr(), false, true);
        return;
      case clang::UO_AddrOf:
        mark(Op->getSubExpr(), true, true);
        return;
      default:
        mark(Op->getSubExpr(), MarkSelf, MarkPointee);
        return;
      }
    }

    if (auto *Member = llvm::dyn_cast<clang::MemberExpr>(Expr)) {
      if (Member->isArrow()) {
        mark(Member->getBase(), false, true);
      } else {
        mark(Member->getBase(), true, false);
      }
      return;
    }

    if (auto *Subscript = llvm::dyn_cast<clang::ArraySubscriptExpr>(Expr)) {
      mark(Subscript->getBase(), false, true);
      return;
    }
  }

  void analyzeArgument(clang::Expr *Arg, clang::QualType ParamType) {
    Arg = ignoreWrappers(Arg);
    if (!Arg) {
      return;
    }

    if (ParamType->isLValueReferenceType()) {
      const clang::QualType Referenced = ParamType.getNonReferenceType();
      if (Referenced.isConstQualified()) {
        return;
      }

      if (Referenced->isPointerType()) {
        mark(Arg, true, true);
      } else {
        mark(Arg, true, false);
      }
      return;
    }

    if (ParamType->isPointerType() &&
        !ParamType->getPointeeType().isConstQualified()) {
      mark(Arg, false, true);
    }
  }

  std::string buildReplacementType(const clang::VarDecl *Decl,
                                   const MutationState &State) const {
    clang::QualType Type = Decl->getType();
    clang::PrintingPolicy Policy(Context.getLangOpts());

    if (Type->isLValueReferenceType()) {
      const clang::QualType Referenced = Type.getNonReferenceType();
      if (Referenced.isConstQualified() || Referenced->isFunctionType() ||
          State.PointeeMutated) {
        return {};
      }

      const clang::QualType NewType = Context.getLValueReferenceType(
          Context.getConstType(Type.getNonReferenceType()));
      return NewType.getAsString(Policy);
    }

    if (!Type->isPointerType()) {
      return {};
    }

    if (Type->getPointeeType()->isFunctionType()) {
      return {};
    }

    if (State.SelfMutated || State.PointeeMutated) {
      return {};
    }

    clang::QualType NewType =
        Context.getPointerType(Context.getConstType(Type->getPointeeType()));
    clang::Qualifiers Quals = Type.getLocalQualifiers();
    Quals.addConst();
    NewType = Context.getQualifiedType(NewType.getUnqualifiedType(), Quals);
    return NewType.getAsString(Policy);
  }
};

class ConstQualifierConsumer : public clang::ASTConsumer {
public:
  explicit ConstQualifierConsumer(clang::CompilerInstance &CI)
      : Rewriter(CI.getSourceManager(), CI.getLangOpts()) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    ConstQualifierVisitor Visitor(Context, Rewriter);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.applyRewrites();

    const clang::FileID MainFile = Context.getSourceManager().getMainFileID();
    if (const auto *Buffer = Rewriter.getRewriteBufferFor(MainFile)) {
      llvm::outs() << std::string(Buffer->begin(), Buffer->end());
      return;
    }

    bool Invalid = false;
    llvm::StringRef Source =
        Context.getSourceManager().getBufferData(MainFile, &Invalid);
    if (!Invalid) {
      llvm::outs() << Source;
    }
  }

private:
  clang::Rewriter Rewriter;
};

class ConstQualifierAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<ConstQualifierConsumer>(CI);
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<ConstQualifierAction>
    X("kondakov_v_lab1_plugin",
      "Adds const to references and pointers that are not modified");
