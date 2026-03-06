// интерфейс для "потребления" AST дерева; 
// точка входа, когда Clang закончит парсинг файла
#include "clang/AST/ASTConsumer.h"

// обход AST дерева;
// чтобы ходить не руками, а просто сказать:
// "Вызови мой метод, когда встретишь ..."
#include "clang/AST/RecursiveASTVisitor.h"

// дает доступ к инфрастуктуре компилятора
// (настройки, диагностика, контекст)
#include "clang/Frontend/CompilerInstance.h"

// чтобы зарегистрировать код как плагин
#include "clang/Frontend/FrontendPluginRegistry.h" 

// аналог iostream; для вывода данных
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <set>

// работа плагина:
// Action (создает) -> Consumer(запускает) -> Visitor(анализирует)


namespace {

  // класс VotincevDVisitor - анализатор
  // просматривает узлы дерева
class VotincevDVisitor final : public clang::RecursiveASTVisitor<VotincevDVisitor> {
public:
  explicit VotincevDVisitor(clang::ASTContext *context) : m_context(context) {}

  // вызывается автоматически для каждого объявления функции в коде
  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    // если у функции есть тело - очищаем списки,
    // чтобы анализировать каждую функцию независимо внутри TU
    if (func->hasBody()) {
      // m_allocatedVars.clear();
      //m_deallocatedVars.clear();
      //m_reportedVars.clear(); // очищаем набор уже отправленных варнингов
    }
    return true;
  }



  // вызывается, когда компилятор встречает return
  // позволяет найти ресурсы, которые не гарантированно освобождаются при выходе
  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    for (auto* allocVar : m_allocatedVars) {
      bool found = false;
      for (auto* deallocVar : m_deallocatedVars) {
        if (allocVar == deallocVar) {
          found = true;
          break;
        }
      }

      // если на момент return переменная не в списке освобожденных
      // и мы о ней еще не сообщали
      if (!found && m_reportedVars.find(allocVar) == m_reportedVars.end()) {
        clang::DiagnosticsEngine& DE = m_context->getDiagnostics();
        unsigned diagID = DE.getCustomDiagID(
            clang::DiagnosticsEngine::Warning, 
            "Ресурс для переменной '%0' может быть не освобожден (не гарантированное освобождение при return)!"
        );
        DE.Report(ret->getReturnLoc(), diagID) << allocVar->getNameAsString();
        
        // запоминаем, что на эту переменную варнинг уже был
        m_reportedVars.insert(allocVar);
      }
    }
    return true;
  }

  // вызывается, когда компилятор видит вызов функции 
  // в моем случае внутри определяется: это free/fclose?
  bool VisitCallExpr(clang::CallExpr *call) {
    // получаем функцию, которую вызывают (Callee)
    clang::FunctionDecl *func = call->getDirectCallee();

    // проверяем, что это не какой-нибудь странный вызов по указателю
    if(!func) {
      return true;
    }
    
    llvm::StringRef funcName = func->getName();
    if(funcName == "free" || funcName == "fclose") {
      // вытаскиваем то, что передали в скобках
      clang::Expr* arg = call->getArg(0)->IgnoreParenCasts();

      // то, что в скобках - это ссылка на переменную?
      clang::VarDecl* var = getVarDeclFromExpr(arg);
      
      // если нашли переменную
      if (var) {
        // llvm::errs() << "Освобождение (" << funcName << "): " << var->getNameAsString() << "\n";
        m_deallocatedVars.push_back(var);
      }
    }
    
    return true;
  }

  // вызывается когда компилятор видит delete
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    // достаем аргумент (то, что после слова delete)
    clang::Expr* arg = del->getArgument()->IgnoreParenCasts();

    // является ли ссылкой на переменную
    clang::VarDecl* var = getVarDeclFromExpr(arg);

    // если ссылка на переменную
    if (var) {
        // llvm::errs() << "Освобождение (delete): " << var->getNameAsString() << "\n";
        m_deallocatedVars.push_back(var);
    }
    return true;
  }

  // вызывается когда компилятор видит объявление новой переменной
  bool VisitVarDecl(clang::VarDecl* var) { 
    clang::Expr* init = var->getInit();
    if (init && isAllocation(init)) {
        // Проверка на уникальность, чтобы не дублировать глобальные/локальные
        if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) == m_allocatedVars.end()) {
            // llvm::errs() << "Нашел выделение (Decl): " << var->getNameAsString() << "\n";
            m_allocatedVars.push_back(var);
        }
    }
    return true;
  } 

  // вызывается когда компилятор видит оператор присваивания (a = ...)
  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    if (op->isAssignmentOp() && isAllocation(op->getRHS())) {
        clang::VarDecl* var = getVarDeclFromExpr(op->getLHS());
        if (var) {
            if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) == m_allocatedVars.end()) {
                // llvm::errs() << "Нашел выделение (Assign): " << var->getNameAsString() << "\n";
                m_allocatedVars.push_back(var);
            }
        }
    }
    return true;
  }

  // функция для сравнения списков и вывода предупреждений
  void checkLeaks() {
    for (auto* allocVar : m_allocatedVars) {
        // если мы уже ругались на эту переменную в VisitReturnStmt — пропускаем
        if (m_reportedVars.count(allocVar)) continue;

        bool found = false;
        for (auto* deallocVar : m_deallocatedVars) {
            if (allocVar == deallocVar) {
                found = true;
                break;
            }
        }

        if (!found) {
            clang::DiagnosticsEngine& DE = m_context->getDiagnostics();
            unsigned diagID = DE.getCustomDiagID(
                clang::DiagnosticsEngine::Warning, 
                "Память или ресурс для переменной '%0' не освобождены!"
            );
            DE.Report(allocVar->getLocation(), diagID) << allocVar->getNameAsString();
        }
    }
  }


private:
  // вспомогательная функция: проверяет, является ли выражение выделением
  bool isAllocation(clang::Expr* e) {
    if (!e) return false;
    clang::Expr* coreExpr = e->IgnoreParenCasts();

    // проверка на вызов функции (malloc/fopen)
    if (clang::CallExpr* call = clang::dyn_cast<clang::CallExpr>(coreExpr)) {
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

  // вспомогательная функция: достает VarDecl из любого выражения (ссылки)
  clang::VarDecl* getVarDeclFromExpr(clang::Expr* e) {
    if (!e) return nullptr;
    clang::Expr* coreExpr = e->IgnoreParenCasts();
    if (clang::DeclRefExpr* ref = clang::dyn_cast<clang::DeclRefExpr>(coreExpr)) {
        return clang::dyn_cast<clang::VarDecl>(ref->getDecl());
    }
    return nullptr;
  }

  clang::ASTContext *m_context;
  std::vector<clang::VarDecl*> m_allocatedVars;
  std::vector<clang::VarDecl*> m_deallocatedVars;
  std::set<clang::VarDecl*> m_reportedVars; // набор переменных, о которых уже выдано предупреждение
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

    // проверяем утечки после обхода
    m_visitor.checkLeaks();
  }

private:
  VotincevDVisitor m_visitor;
};


// точка входа
class ExampleAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  // метод создания экземпляра нашего Consumer
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<VotincevDConsumer>(&ci.getASTContext());
  }

  // позволяет плагину принимать аргументы из командной строки
  bool ParseArgs(const clang::CompilerInstance &ci,
                  const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

// регистрация плагина (чтобы его видел Clang)
static clang::FrontendPluginRegistry::Add<ExampleAction>
    X("votincev_d_analyzerplugin", "Description plugin");