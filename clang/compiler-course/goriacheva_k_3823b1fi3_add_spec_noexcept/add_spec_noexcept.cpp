#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class NoexceptVisitor final
    : public clang::RecursiveASTVisitor<NoexceptVisitor> {
public:
  explicit NoexceptVisitor(clang::ASTContext *context) : m_context(context) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func) {

    if (!func->hasBody())
      return true;

    auto *Proto = func->getType()->getAs<clang::FunctionProtoType>();
    if (!Proto)
      return true;

    if (Proto->getExceptionSpecType() != clang::EST_None) {
      func->dump();
      return true;
    }

    if (!bodyMayThrow(func->getBody())) {
      addNoexcept(func, Proto);
    }

    func->dump();
    return true;
  }

private:
  clang::ASTContext *m_context;

  bool bodyMayThrow(clang::Stmt *Body) {

    for (auto *Child : Body->children()) {
      if (!Child)
        continue;

      if (llvm::isa<clang::CXXThrowExpr>(Child))
        return true;

      if (llvm::isa<clang::CXXNewExpr>(Child))
        return true;

      if (auto *Call = llvm::dyn_cast<clang::CallExpr>(Child)) {

        auto *Callee = Call->getDirectCallee();
        if (!Callee)
          return true;

        auto *Proto = Callee->getType()->getAs<clang::FunctionProtoType>();

        if (!Proto)
          return true;

        auto Spec = Proto->getExceptionSpecType();

        if (Spec != clang::EST_BasicNoexcept && Spec != clang::EST_NoThrow &&
            Spec != clang::EST_NoexceptTrue)
          return true;
      }

      if (bodyMayThrow(Child))
        return true;
    }

    return false;
  }

  void addNoexcept(clang::FunctionDecl *FD,
                   const clang::FunctionProtoType *Proto) {

    clang::FunctionProtoType::ExtProtoInfo Info = Proto->getExtProtoInfo();

    Info.ExceptionSpec.Type = clang::EST_BasicNoexcept;

    clang::QualType NewType = m_context->getFunctionType(
        Proto->getReturnType(), Proto->getParamTypes(), Info);

    FD->setType(NewType);
  }
};

class NoexceptConsumer final : public clang::ASTConsumer {
public:
  explicit NoexceptConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
  }

private:
  NoexceptVisitor m_visitor;
};

class ActionAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<NoexceptConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<ActionAction> X("spec_noexcept",
                                                          "Description plugin");
