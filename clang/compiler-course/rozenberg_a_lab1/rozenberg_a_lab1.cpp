#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class MutationFinder : public clang::RecursiveASTVisitor<MutationFinder> {
  clang::VarDecl *Target;
  bool Mutated = false;

public:
  MutationFinder(clang::VarDecl *D) : Target(D) {}

  bool isMutated() const { return Mutated; }

  bool VisitDeclRefExpr(clang::DeclRefExpr *Node) {
    if (Node->getDecl() != Target)
      return true;

    // check how variable is used
    auto &Ctx = Target->getASTContext();
    clang::DynTypedNode CurrentNode = clang::DynTypedNode::create(*Node);
    bool isDeref = Target->getType()->isReferenceType();

    while (true) {
      auto Parents = Ctx.getParents(CurrentNode);
      if (Parents.empty())
        break;

      const clang::Stmt *S = Parents[0].get<clang::Stmt>();
      if (!S)
        break;

      //  skip brackets and implicit casts
      if (clang::isa<clang::ImplicitCastExpr>(S) ||
          clang::isa<clang::ParenExpr>(S)) {
        CurrentNode = Parents[0];
        continue;
      }

      //  set flag if met with dereferencing
      if (auto *UO = clang::dyn_cast<clang::UnaryOperator>(S)) {
        if (UO->getOpcode() == clang::UO_Deref) {
          isDeref = true;
          CurrentNode = Parents[0];
          continue;
        }
      }
      if (clang::isa<clang::ArraySubscriptExpr>(S)) {
        isDeref = true;
        CurrentNode = Parents[0];
        continue;
      }

      //  check mutation
      bool isAssignment = false;
      if (auto *BO = clang::dyn_cast<clang::BinaryOperator>(S)) {
        if (BO->isAssignmentOp() && BO->getLHS()->IgnoreParenImpCasts() ==
                                        CurrentNode.get<clang::Expr>()) {
          isAssignment = true;
        }
      }
      if (auto *UO = clang::dyn_cast<clang::UnaryOperator>(S)) {
        if (UO->isIncrementDecrementOp()) {
          isAssignment = true;
        }
      }

      if (isAssignment) {
        if (isDeref) {
          Mutated = true;
          return false;
        }
      }
      break;
    }
    return true;
  }
};

class ConstCorrVisitor final
    : public clang::RecursiveASTVisitor<ConstCorrVisitor> {
  clang::ASTContext *context;
  clang::Rewriter &rewriter;

public:
  explicit ConstCorrVisitor(clang::ASTContext *context,
                            clang::Rewriter &rewriter)
      : context(context), rewriter(rewriter) {}

  bool VisitVarDecl(clang::VarDecl *D) {
    clang::QualType T = D->getType();
    //  check if var is const or if not pointer/reference
    if (T.isConstQualified() ||
        (!T->isPointerType() && !T->isReferenceType())) {
      return true;
    }

    //  check if pointer data is const
    if (T->isPointerType() && T->getPointeeType().isConstQualified()) {
      return true;
    }

    //  get context where variable is used
    clang::DeclContext *DC = D->getDeclContext();
    clang::Stmt *body = nullptr;
    if (clang::FunctionDecl *FD = clang::dyn_cast<clang::FunctionDecl>(DC)) {
      body = FD->getBody();
    } else if (auto *MD = clang::dyn_cast<clang::CXXMethodDecl>(DC)) {
      body = MD->getBody();
    }

    //  check if pointer data was mutated
    if (body) {
      MutationFinder Analyzer(D);
      Analyzer.TraverseStmt(body);

      if (!Analyzer.isMutated()) {
        applyConstFix(D);
      }
    }
    return true;
  }

private:
  void applyConstFix(clang::VarDecl *D) {
    clang::SourceLocation Loc =
        D->getTypeSourceInfo()->getTypeLoc().getBeginLoc();

    if (Loc.isValid() && !Loc.isMacroID()) {
      rewriter.InsertText(Loc, "const ", true, false);
    }
  }
};

class ConstCorrConsumer final : public clang::ASTConsumer {
  clang::Rewriter rewriter;
  clang::ASTContext *context;

public:
  void Initialize(clang::ASTContext &C) override {
    context = &C;
    rewriter.setSourceMgr(C.getSourceManager(), C.getLangOpts());
  }

  void HandleTranslationUnit(clang::ASTContext &C) override {
    ConstCorrVisitor visitor(&C, rewriter);
    visitor.TraverseDecl(C.getTranslationUnitDecl());

    clang::SourceManager &SM = C.getSourceManager();
    clang::FileID mainFileID = SM.getMainFileID();
    const llvm::RewriteBuffer *RewriteBuf =
        rewriter.getRewriteBufferFor(mainFileID);

    if (RewriteBuf) {
      llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    } else {
      bool invalid = false;
      llvm::StringRef buf = SM.getBufferData(mainFileID, &invalid);
      if (!invalid)
        llvm::outs() << buf;
    }
    llvm::outs().flush();
  }
};

class ConstCorrAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    ci.getDiagnostics().getDiagnosticOptions().ShowCarets = false;
    return std::make_unique<ConstCorrConsumer>();
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<ConstCorrAction>
    X("rozenberg_a_plugin_1", "Const correctness plugin for lab 1");
