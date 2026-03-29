#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>
#include <vector>

namespace {

struct VolkovA_Metrics {
  unsigned Globals = 0;
  unsigned Statics = 0;
  unsigned Locals = 0;
  unsigned Params = 0;

  unsigned GetTotal() const { return Globals + Statics + Locals + Params; }
};

class VolkovAVarStatVisitor final
    : public clang::RecursiveASTVisitor<VolkovAVarStatVisitor> {
public:
  // отключаем обход, чтобы не считать их переменные повторно
  bool shouldVisitTemplateInstantiations() const { return false; }

  bool VisitVarDecl(clang::VarDecl *Declaration) {
    // 1. игнорим чистые объявления без определения (например, 'extern int x;')
    // => нет проблемы дублирования глобальных переменных из заголовочных файлов
    if (Declaration->isThisDeclarationADefinition() ==
        clang::VarDecl::DeclarationOnly) {
      return true;
    }

    // 2. чекаем параметры функции
    if (auto *Param = llvm::dyn_cast<clang::ParmVarDecl>(Declaration)) {
      // игнорим параметры внутри предварительных объявлений функций (например:
      // int foo(int a);)
      if (auto *Func =
              llvm::dyn_cast<clang::FunctionDecl>(Param->getDeclContext())) {
        if (!Func->isThisDeclarationADefinition()) {
          return true;
        }
      }
      Metrics.Params++;
      return true;
    }

    // 3. распределяем локальные переменные
    if (Declaration->isLocalVarDecl()) {
      if (Declaration->isStaticLocal()) {
        Metrics.Statics++;
      } else {
        Metrics.Locals++;
      }
    }
    // 4. распределяем файловые переменные (глобальные и статические вне
    // функций)
    else if (Declaration->isFileVarDecl()) {
      if (Declaration->getStorageClass() == clang::SC_Static) {
        Metrics.Statics++;
      } else {
        Metrics.Globals++;
      }
    }

    return true;
  }

  void PrintReport() const {
    llvm::outs() << "Total count : " << Metrics.GetTotal() << "\n";
    llvm::outs() << "Global variables : " << Metrics.Globals << "\n";
    llvm::outs() << "Static variables : " << Metrics.Statics << "\n";
    llvm::outs() << "Local variables  : " << Metrics.Locals << "\n";
    llvm::outs() << "Function params  : " << Metrics.Params << "\n";
  }

private:
  VolkovA_Metrics Metrics;
};

class VolkovAVarStatConsumer final : public clang::ASTConsumer {
public:
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.PrintReport();
  }

private:
  VolkovAVarStatVisitor Visitor;
};

class VolkovAVarStatAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler,
                    llvm::StringRef InFile) override {
    return std::make_unique<VolkovAVarStatConsumer>();
  }

  bool ParseArgs(const clang::CompilerInstance &Compiler,
                 const std::vector<std::string> &Args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<VolkovAVarStatAction>
    X("volkov_a_var_statistic", "Counts variables by their scope (Volkov A)");