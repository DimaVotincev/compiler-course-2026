#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct Statistic // здесь будет храниться статистика по переменным
{
  int global_obj = 0;
  int static_vars = 0;
  int local_vars = 0;
  int params = 0;
};

class StatisticVisitor final
    : public clang::RecursiveASTVisitor<StatisticVisitor> {
public:
  explicit StatisticVisitor(clang::ASTContext *context)
      : m_context(context), stat(Statistic()) {}

  // вызывается, когда доходим до пар-в ф-ии
  bool VisitParmVarDecl(clang::ParmVarDecl *D) {
    stat.params++;
    return true;
  }

  // вызывается, когда доходим до обьявления переменных
  bool VisitVarDecl(clang::VarDecl *D) {
    // т.к. пар-ры ф-ии также явл-ся переменными, проверяем, не они ли это.
    // Иначе посчитаем дважды
    if (clang::isa<clang::ParmVarDecl>(D)) {
      return true;
    }

    if (D->getStorageClass() == clang::SC_Static || D->isStaticDataMember()) {
      stat.static_vars++;
    } else if (D->isFileVarDecl()) {
      stat.global_obj++;
    } else if (D->isLocalVarDecl()) {
      stat.local_vars++;
    }

    return true;
  }

  Statistic get_statistic() const { return stat; }

private:
  clang::ASTContext *m_context;
  Statistic stat;
};

class StatisticConsumer final : public clang::ASTConsumer {
public:
  explicit StatisticConsumer(clang::ASTContext *context) : m_visitor(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());

    Statistic stat = m_visitor.get_statistic();

    auto &out = llvm::outs();
    out << "\nStatistics\n";
    out << "Global objects: " << stat.global_obj << "\n";
    out << "Local variables: " << stat.local_vars << "\n";
    out << "Static variables: " << stat.static_vars << "\n";
    out << "Params: " << stat.params << "\n";
  }

private:
  StatisticVisitor m_visitor;
};

class StatisticAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<StatisticConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

static clang::FrontendPluginRegistry::Add<StatisticAction>
    X("VariableStatisticsPlugin",
      "Plugin for obtaining statistics on variables in TU");
