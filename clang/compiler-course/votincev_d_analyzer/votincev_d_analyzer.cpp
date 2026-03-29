#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <vector>

// работа плагина:
// Action (создает) -> Consumer(запускает) -> Visitor(анализирует)

namespace {

// класс VotincevDVisitor - анализатор
// просматривает узлы дерева
class VotincevDVisitor final
    : public clang::RecursiveASTVisitor<VotincevDVisitor> {
public:
  explicit VotincevDVisitor(clang::ASTContext *context) : m_context(context) {}

  // находим ресурсы, которые не гарантированно освобождаются при выходе
  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    for (auto *allocVar : m_allocatedVars) {
      bool found = false;
      for (auto *deallocVar : m_deallocatedVars) {
        if (allocVar == deallocVar) {
          found = true;
          break;
        }
      }

      // если на момент return переменная не в списке освобожденных
      // и мы о ней еще не сообщали
      if (!found && m_reportedVars.find(allocVar) == m_reportedVars.end()) {
        clang::DiagnosticsEngine &DE = m_context->getDiagnostics();
        unsigned diagID = DE.getCustomDiagID(
            clang::DiagnosticsEngine::Warning,
            "Ресурс для переменной '%0' может быть не освобожден (не "
            "гарантированное освобождение при return)!");
        DE.Report(ret->getReturnLoc(), diagID) << allocVar->getNameAsString();

        m_reportedVars.insert(allocVar);
      }
    }
    return true;
  }

  // функция free/fclose?
  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *func = call->getDirectCallee();

    // проверяем, что это не какой-нибудь странный вызов по указателю
    if (!func) {
      return true;
    }

    llvm::StringRef funcName = func->getName();
    if (funcName == "free" || funcName == "fclose") {
      clang::Expr *arg = call->getArg(0)->IgnoreParenCasts();

      clang::VarDecl *var = getVarDeclFromExpr(arg);

      // если нашли переменную
      if (var) {
        m_deallocatedVars.push_back(var);
      }
    }

    return true;
  }

  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    clang::Expr *arg = del->getArgument()->IgnoreParenCasts();
    clang::VarDecl *var = getVarDeclFromExpr(arg);
    if (var) {
      m_deallocatedVars.push_back(var);
    }
    return true;
  }

  bool VisitVarDecl(clang::VarDecl *var) {
    clang::Expr *init = var->getInit();
    // если инициализация есть и это выделение памяти (malloc/new)
    if (init && isAllocation(init)) {
      // Проверка на уникальность, чтобы не дублировать глобальные/локальные
      if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) ==
          m_allocatedVars.end()) {
        m_allocatedVars.push_back(var);
      }
    }
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    // если это операция присваивания и справа стоит выделение памяти
    if (op->isAssignmentOp() && isAllocation(op->getRHS())) {
      clang::VarDecl *var = getVarDeclFromExpr(op->getLHS());
      if (var) {
        // если этой переменной еще нет в списке выделенных - добавляем
        if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) ==
            m_allocatedVars.end()) {
          m_allocatedVars.push_back(var);
        }
      }
    }
    return true;
  }

  // функция для сравнения списков и вывода предупреждений
  void checkLeaks() {
    for (auto *allocVar : m_allocatedVars) {
      // если мы уже ругались на эту переменную в VisitReturnStmt — пропускаем
      if (m_reportedVars.count(allocVar))
        continue;

      bool found = false; // флаг: нашли ли удаление

      for (auto *deallocVar : m_deallocatedVars) {
        if (allocVar == deallocVar) {
          found = true; // нашли, значит утечки нет
          break;
        }
      }

      // если дошли до конца и удаления не нашли
      if (!found) {
        clang::DiagnosticsEngine &DE = m_context->getDiagnostics();
        unsigned diagID = DE.getCustomDiagID(
            clang::DiagnosticsEngine::Warning,
            "Память или ресурс для переменной '%0' не освобождены!");
        DE.Report(allocVar->getLocation(), diagID)
            << allocVar->getNameAsString();
      }
    }
  }

private:
  // проверяет, является ли выражение выделением
  bool isAllocation(clang::Expr *e) {
    if (!e)
      return false;
    clang::Expr *coreExpr = e->IgnoreParenCasts();

    // проверка на вызов функции (malloc/fopen)
    if (clang::CallExpr *call = clang::dyn_cast<clang::CallExpr>(coreExpr)) {
      if (clang::FunctionDecl *func = call->getDirectCallee()) {
        llvm::StringRef funcName = func->getName();
        return (funcName == "malloc" || funcName == "fopen");
      }
    }

    // проверка на оператор new
    if (clang::isa<clang::CXXNewExpr>(coreExpr)) {
      return true;
    }

    return false;
  }

  // достает VarDecl из любого выражения (ссылки)
  clang::VarDecl *getVarDeclFromExpr(clang::Expr *e) {
    if (!e)
      return nullptr;
    clang::Expr *coreExpr = e->IgnoreParenCasts();
    // если это ссылка на объявление (DeclRef)
    if (clang::DeclRefExpr *ref =
            clang::dyn_cast<clang::DeclRefExpr>(coreExpr)) {
      // пытаемся превратить это в объявление переменной (VarDecl)
      return clang::dyn_cast<clang::VarDecl>(ref->getDecl());
    }
    return nullptr; // если это не переменная - возвращаем пустоту
  }

  clang::ASTContext *m_context; // контекст для доступа к данным компиляции
  std::vector<clang::VarDecl *>
      m_allocatedVars; // список переменных, создавших ресурсы
  std::vector<clang::VarDecl *>
      m_deallocatedVars; // список переменных, удаливших ресурсы
  std::set<clang::VarDecl *>
      m_reportedVars; // набор переменных, о которых уже выдано предупреждение
};

// класс-посредник (между Action и Visitor)
class VotincevDConsumer final : public clang::ASTConsumer {
public:
  explicit VotincevDConsumer(clang::ASTContext *context) : m_visitor(context) {}

  // вызывается 1 раз когда весь TU полностью разобран в AST
  void HandleTranslationUnit(clang::ASTContext &context) override {
    // берем корень дерева (TranslationUnitDecl)
    // и запускаме нашего Visitor m_visitor гулять по узлам
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());

    // проверяем утечки после того, как обошли весь файл
    m_visitor.checkLeaks();
  }

private:
  VotincevDVisitor m_visitor;
};

// точка входа
class ExampleAction final : public clang::PluginASTAction {
public:
  // метод создания экземпляра нашего Consumer, который будет "потреблять"
  // дерево
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    // отдаем компилятору наш Consumer, привязанный к текущему контексту
    return std::make_unique<VotincevDConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

// регистрация плагина (чтобы его видел Clang под коротким именем)
static clang::FrontendPluginRegistry::Add<ExampleAction>
    X("votincev_d_analyzerplugin", "Static analyzer for memory leaks and "
                                   "resource management (malloc/new/fopen)");
