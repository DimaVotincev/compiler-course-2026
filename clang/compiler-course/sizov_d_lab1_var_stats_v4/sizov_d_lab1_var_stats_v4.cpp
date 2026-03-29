#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace {
class SizovVarStatsVisitor final
    : public clang::RecursiveASTVisitor<SizovVarStatsVisitor> {
public:
  explicit SizovVarStatsVisitor(clang::ASTContext &context)
      : Context(context) {}

  bool VisitVarDecl(clang::VarDecl *varDecl) {
    if (!shouldCountDecl(varDecl) || varDecl->isImplicit()) {
      return true;
    }

    if (llvm::isa<clang::ParmVarDecl>(varDecl)) {
      return true;
    }

    if (!varDecl->isThisDeclarationADefinition()) {
      return true;
    }

    if (varDecl->isStaticLocal() || varDecl->isStaticDataMember() ||
        (varDecl->isFileVarDecl() &&
         varDecl->getStorageClass() == clang::SC_Static)) {
      ++StaticCount;
      return true;
    }

    if (varDecl->isLocalVarDecl()) {
      ++LocalCount;
      return true;
    }

    if (varDecl->hasGlobalStorage()) {
      ++GlobalCount;
    }

    return true;
  }

  bool VisitParmVarDecl(clang::ParmVarDecl *parmVarDecl) {
    if (!shouldCountDecl(parmVarDecl) || parmVarDecl->isImplicit()) {
      return true;
    }

    const auto *func =
        llvm::dyn_cast<clang::FunctionDecl>(parmVarDecl->getDeclContext());
    if (!func || !func->isThisDeclarationADefinition()) {
      return true;
    }

    ++ParameterCount;
    return true;
  }

  void PrintSummary() {
    auto &out = llvm::outs();
    out << "Variable stats for TU:\n";
    if (TotalCount() == 0) {
      out << "  (no variables found)\n";
      return;
    }

    out << "Global objects: " << GlobalCount << "\n";
    out << "Local variables: " << LocalCount << "\n";
    out << "Static variables: " << StaticCount << "\n";
    out << "Function parameters: " << ParameterCount << "\n";
  }

private:
  clang::ASTContext &Context;
  unsigned GlobalCount = 0;
  unsigned StaticCount = 0;
  unsigned LocalCount = 0;
  unsigned ParameterCount = 0;

  [[nodiscard]] unsigned TotalCount() const {
    return GlobalCount + StaticCount + LocalCount + ParameterCount;
  }

  [[nodiscard]] bool shouldCountDecl(const clang::Decl *decl) const {
    if (!decl) {
      return false;
    }

    auto loc = decl->getLocation();
    if (loc.isInvalid()) {
      return false;
    }

    const auto &sm = Context.getSourceManager();
    loc = sm.getExpansionLoc(loc);
    return sm.isWrittenInMainFile(loc) && !sm.isInSystemHeader(loc) &&
           !sm.isInSystemMacro(loc);
  }
};

class SizovVarStatsConsumer final : public clang::ASTConsumer {
public:
  explicit SizovVarStatsConsumer(clang::ASTContext &context)
      : Visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    Visitor.TraverseDecl(context.getTranslationUnitDecl());
    Visitor.PrintSummary();
  }

private:
  SizovVarStatsVisitor Visitor;
};

class SizovVarStatsAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci,
                    llvm::StringRef /* unused */) override {
    return std::make_unique<SizovVarStatsConsumer>(ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<SizovVarStatsAction>
    X("SizovDLab1VarStatsV4Plugin_Sizov_D_FIIT2_ClangAST",
      "Print variable statistics for the whole translation unit");
