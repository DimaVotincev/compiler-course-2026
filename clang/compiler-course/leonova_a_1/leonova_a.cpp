#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

namespace {

class SafetyVisitor : public clang::RecursiveASTVisitor<SafetyVisitor> {
private:
  clang::ASTContext &Context;
  bool mayThrow;

public:
  explicit SafetyVisitor(clang::ASTContext &ctx)
      : Context(ctx), mayThrow(false) {}

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *) {
    mayThrow = true;
    return false;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *callee = call->getDirectCallee();
    if (!callee) {
      mayThrow = true;
      return false;
    }

    const clang::FunctionProtoType *proto =
        callee->getType()->getAs<clang::FunctionProtoType>();
    if (!proto) {
      mayThrow = true;
      return false;
    }

    auto spec = proto->getExceptionSpecType();
    bool isNoexcept =
        (spec == clang::EST_BasicNoexcept || spec == clang::EST_NoThrow);
    if (!isNoexcept) {
      mayThrow = true;
      return false;
    }

    return true;
  }

  bool functionMayThrow() const { return mayThrow; }
};

clang::QualType makeNoexceptType(clang::ASTContext &ctx,
                                 const clang::FunctionProtoType *proto) {
  clang::FunctionProtoType::ExtProtoInfo info = proto->getExtProtoInfo();
  info.ExceptionSpec.Type = clang::EST_BasicNoexcept;

  return ctx.getFunctionType(proto->getReturnType(), proto->getParamTypes(),
                             info);
}

class NoexceptAnnotator : public clang::RecursiveASTVisitor<NoexceptAnnotator> {
public:
  explicit NoexceptAnnotator(clang::ASTContext &ctx) : Context(ctx) {}

  bool VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (!FD->hasBody())
      return true;

    const clang::FunctionProtoType *proto =
        FD->getType()->getAs<clang::FunctionProtoType>();
    if (!proto)
      return true;

    if (proto->getExceptionSpecType() != clang::EST_None)
      return true;

    SafetyVisitor analyzer(Context);
    analyzer.TraverseStmt(FD->getBody());

    if (!analyzer.functionMayThrow()) {
      clang::QualType newType = makeNoexceptType(Context, proto);
      FD->setType(newType);
    }

    return true;
  }

private:
  clang::ASTContext &Context;
};

class PluginConsumer : public clang::ASTConsumer {
public:
  explicit PluginConsumer(clang::ASTContext &ctx) : Annotator(ctx) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    Annotator.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  NoexceptAnnotator Annotator;
};

class PluginAction : public clang::PluginASTAction {
protected:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<PluginConsumer>(CI.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  ActionType getActionType() override { return AddBeforeMainAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<PluginAction>
    X("add_noexcept_plugin", "Add noexcept to safe functions");
