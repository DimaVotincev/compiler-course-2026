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

  // вызывается, когда компилятор встречает return
  // позволяет найти ресурсы, которые не гарантированно освобождаются при выходе
  bool VisitReturnStmt(clang::ReturnStmt *ret) {
    // проходим по списку всех переменных, которым выделили память
    for (auto* allocVar : m_allocatedVars) {
      bool found = false; // флаг: нашли ли мы освобождение для этой переменной
      // ищем эту переменную в списке тех, что были удалены (free/delete)
      for (auto* deallocVar : m_deallocatedVars) {
        if (allocVar == deallocVar) {
          found = true; // если нашли совпадение - всё хорошо
          break;
        }
      }

      // если на момент return переменная не в списке освобожденных
      // и мы о ней еще не сообщали
      if (!found && m_reportedVars.find(allocVar) == m_reportedVars.end()) {
        // получаем движок диагностики, чтобы рисовать варнинги в консоли
        clang::DiagnosticsEngine& DE = m_context->getDiagnostics();
        // создаем уникальный номер (ID) для нашего собственного сообщения об ошибке
        unsigned diagID = DE.getCustomDiagID(
            clang::DiagnosticsEngine::Warning, 
            "Ресурс для переменной '%0' может быть не освобожден (не гарантированное освобождение при return)!"
        );
        // выводим варнинг в месте, где стоит return, подставляя имя переменной
        DE.Report(ret->getReturnLoc(), diagID) << allocVar->getNameAsString();
        
        // запоминаем, что на эту переменную варнинг уже был
        m_reportedVars.insert(allocVar);
      }
    }
    return true; // продолжаем обход дерева
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
    
    // берем текстовое имя функции
    llvm::StringRef funcName = func->getName();
    // если это функции очистки памяти или закрытия файла
    if(funcName == "free" || funcName == "fclose") {
      // вытаскиваем то, что передали в скобках, игнорируя скобочки и приведение типов
      clang::Expr* arg = call->getArg(0)->IgnoreParenCasts();

      // то, что в скобках - это ссылка на переменную?
      clang::VarDecl* var = getVarDeclFromExpr(arg);
      
      // если нашли переменную
      if (var) {
        // закидываем её в список "освобожденных"
        m_deallocatedVars.push_back(var);
      }
    }
    
    return true; // продолжаем обход дерева
  }

  // вызывается когда компилятор видит delete
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    // достаем аргумент (то, что после слова delete), чистим от лишних кастов
    clang::Expr* arg = del->getArgument()->IgnoreParenCasts();

    // является ли ссылкой на переменную
    clang::VarDecl* var = getVarDeclFromExpr(arg);

    // если ссылка на переменную
    if (var) {
        // закидываем её в список "освобожденных" через delete
        m_deallocatedVars.push_back(var);
    }
    return true; // продолжаем обход дерева
  }

  // вызывается когда компилятор видит объявление новой переменной
  bool VisitVarDecl(clang::VarDecl* var) { 
    // получаем выражение инициализации (то, что после знака =)
    clang::Expr* init = var->getInit();
    // если инициализация есть и это выделение памяти (malloc/new)
    if (init && isAllocation(init)) {
        // Проверка на уникальность, чтобы не дублировать глобальные/локальные
        if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) == m_allocatedVars.end()) {
            // добавляем переменную в список "подозреваемых" на утечку
            m_allocatedVars.push_back(var);
        }
    }
    return true; // продолжаем обход дерева
  } 

  // вызывается когда компилятор видит оператор присваивания (a = ...)
  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    // если это операция присваивания и справа стоит выделение памяти
    if (op->isAssignmentOp() && isAllocation(op->getRHS())) {
        // пытаемся достать переменную из левой части (куда присваиваем)
        clang::VarDecl* var = getVarDeclFromExpr(op->getLHS());
        if (var) {
            // если этой переменной еще нет в списке выделенных - добавляем
            if (std::find(m_allocatedVars.begin(), m_allocatedVars.end(), var) == m_allocatedVars.end()) {
                m_allocatedVars.push_back(var);
            }
        }
    }
    return true; // продолжаем обход дерева
  }

  // функция для сравнения списков и вывода предупреждений
  void checkLeaks() {
    // проходим по всем переменным, которым когда-либо выделяли ресурс
    for (auto* allocVar : m_allocatedVars) {
        // если мы уже ругались на эту переменную в VisitReturnStmt — пропускаем
        if (m_reportedVars.count(allocVar)) continue;

        bool found = false; // флаг: нашли ли удаление
        // ищем в списке удалений
        for (auto* deallocVar : m_deallocatedVars) {
            if (allocVar == deallocVar) {
                found = true; // нашли, значит утечки нет
                break;
            }
        }

        // если дошли до конца и удаления не нашли
        if (!found) {
            // берем движок диагностики
            clang::DiagnosticsEngine& DE = m_context->getDiagnostics();
            // создаем сообщение о том, что память забыли очистить в принципе
            unsigned diagID = DE.getCustomDiagID(
                clang::DiagnosticsEngine::Warning, 
                "Память или ресурс для переменной '%0' не освобождены!"
            );
            // кидаем варнинг прямо на строку, где была объявлена переменная
            DE.Report(allocVar->getLocation(), diagID) << allocVar->getNameAsString();
        }
    }
  }


private:
  // вспомогательная функция: проверяет, является ли выражение выделением
  bool isAllocation(clang::Expr* e) {
    if (!e) return false;
    // убираем лишние касты и скобки, чтобы увидеть саму суть выражения
    clang::Expr* coreExpr = e->IgnoreParenCasts();

    // проверка на вызов функции (malloc/fopen)
    if (clang::CallExpr* call = clang::dyn_cast<clang::CallExpr>(coreExpr)) {
        if (clang::FunctionDecl *func = call->getDirectCallee()) {
            llvm::StringRef funcName = func->getName();
            // если имя функции совпадает с malloc или fopen - это выделение
            return (funcName == "malloc" || funcName == "fopen");
        }
    }

    // проверка на оператор new (встроенный в язык С++)
    if (clang::isa<clang::CXXNewExpr>(coreExpr)) {
        return true;
    }

    return false; // во всех остальных случаях - это не выделение
  }

  // вспомогательная функция: достает VarDecl из любого выражения (ссылки)
  clang::VarDecl* getVarDeclFromExpr(clang::Expr* e) {
    if (!e) return nullptr;
    // убираем мусор в выражении
    clang::Expr* coreExpr = e->IgnoreParenCasts();
    // если это ссылка на объявление (DeclRef)
    if (clang::DeclRefExpr* ref = clang::dyn_cast<clang::DeclRefExpr>(coreExpr)) {
        // пытаемся превратить это в объявление переменной (VarDecl)
        return clang::dyn_cast<clang::VarDecl>(ref->getDecl());
    }
    return nullptr; // если это не переменная - возвращаем пустоту
  }

  clang::ASTContext *m_context; // контекст для доступа к данным компиляции
  std::vector<clang::VarDecl*> m_allocatedVars; // список переменных, создавших ресурсы
  std::vector<clang::VarDecl*> m_deallocatedVars; // список переменных, удаливших ресурсы
  std::set<clang::VarDecl*> m_reportedVars; // набор переменных, о которых уже выдано предупреждение
};


// класс-посредник (между Action и Visitor)
class VotincevDConsumer final : public clang::ASTConsumer {
public:
  // конструктор: создаем нашего "посетителя"
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
  VotincevDVisitor m_visitor; // наш главный анализатор
};


// точка входа
class ExampleAction final : public clang::PluginASTAction {
public:
  // метод создания экземпляра нашего Consumer, который будет "потреблять" дерево
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    // отдаем компилятору наш Consumer, привязанный к текущему контексту
    return std::make_unique<VotincevDConsumer>(&ci.getASTContext());
  }

  // позволяет плагину принимать аргументы из командной строки (здесь просто возвращаем true)
  bool ParseArgs(const clang::CompilerInstance &ci,
                  const std::vector<std::string> &args) override {
    return true;
  }
};
} // namespace

// регистрация плагина (чтобы его видел Clang под коротким именем)
static clang::FrontendPluginRegistry::Add<ExampleAction>
    X("votincev_d_analyzerplugin", "Static analyzer for memory leaks and resource management (malloc/new/fopen)");