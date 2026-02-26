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

// работа плагина:
// Action (создает) -> Consumer(запускает) -> Visitor(анализирует)

namespace {

  // класс VotincevDVisitor - анализатор
  // просматривает узлы дерева
class VotincevDVisitor final : public clang::RecursiveASTVisitor<VotincevDVisitor> {
public:
  explicit VotincevDVisitor(clang::ASTContext *context) : m_context(context) {}


  // вызывается автоматически для каждого объявления функци в коде
  // все подобные функции работы с классами: Visit+ИмяКлассаУзла
  // bool VisitFunctionDecl(clang::FunctionDecl *func) {

  //   // dump() - метод который печатает структуру узла stderr в текстовом виде
  //   // (например дерево с BinaryOperator и тд)
  //   func->dump();

  //   // return true == продолжить обход дерева
  //   // если вернуть false == обход прервется
  //   return true;
  // }






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
      clang::DeclRefExpr* ref = clang::dyn_cast<clang::DeclRefExpr>(arg);
      // если это ссылка
      if (ref) {

        // достаем само объявление из ссылки
        clang::VarDecl* var = clang::dyn_cast<clang::VarDecl>(ref->getDecl());
        if (var) {
          llvm::errs() << "Освобождение (" << funcName << "): " << var->getNameAsString() << "\n";
          m_deallocatedVars.push_back(var);
        }
      }
    }
    
    return true;
  }

  // вызывается когда компилятор видит new
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *del) {
    // достаем аргумент (то, что после слова delete)
    clang::Expr* arg = del->getArgument()->IgnoreParenCasts();

    // проверяем, ссылка ли это
    clang::DeclRefExpr* ref = clang::dyn_cast<clang::DeclRefExpr>(arg);

    // если это ссылка
    if (ref) {
        // является ли ссылкой на переменную
        clang::VarDecl* var = clang::dyn_cast<clang::VarDecl>(ref->getDecl());

        // если ссылка на переменную
        if (var) {
            llvm::errs() << "Освобождение (delete): " << var->getNameAsString() << "\n";
            m_deallocatedVars.push_back(var);
        }
    }
    return true;
}



  // переменные в Clang представлены как класс VarDecl
  // поэтому чтобы их ловить: VisitVarDecl
  // bool VisitVarDecl(clang::VarDecl* var) { // !!!!!!!!!!!!!!!!!!!!!!!! пример !!!!!!!!!!!!!
  //   // llvm::errs() << "Я нашел переменную: " << var->getNameAsString() << "\n";

  //   return true;

  //   // получаю тип объявленной переменной
  //   clang::QualType var_type = var->getType();

  //   // если переменная не глобальная или не int - не идем дальше
  //   if(!var->getDeclContext()->isTranslationUnit() ||
  //       !var_type->isSpecificBuiltinType(clang::BuiltinType::Int)) {
  //     return true;
  //   }


  //   // получаю движок, чтобы зарегистрировать ID своего warning'a
  //   clang::DiagnosticsEngine& DE = m_context->getDiagnostics();

  //   // получаю ID моего варнинга
  //   // 1: тип (Warning, Error, ....)
  //   // 2: что будет выведено 
  //   // (%* - это куда будет вставляться текст с помощью << при вызове Report)
  //   unsigned my_warn_id = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
  //   "Global 'int' variable Declaration: %0");

  //   // кидаем Warning
  //   // 1: позиция переменной
  //   // 2: ID варна
  //   DE.Report(var->getLocation(),my_warn_id) << var->getNameAsString();
  //   return true;


  //   // у каждой переменной есть метод getType()
  //   // он возвращает объект QualType - умная обертка над типом
  //   // чтобы проверить, являетстя ли переменная родным типом int 
  //   // нужно использовать метод isSpecificBuiltinType(clang::BuiltinType::Int)

  //   /*
  //   Проверка, что переменная - глобальная
  //   if (var->hasGlobalStorage()) {
  //   // Это глобальная переменная (или static переменная)
  //   }

  //   // Или более строгий вариант (проверка, что родитель — это сам файл):
  //   if (var->getDeclContext()->isTranslationUnit()) {
  //       // Это точно глобальная переменная верхнего уровня
  //   }
    
  //   */
  // }
  






  // вызывается когда компилятор видит объявление новой переменной
  bool VisitVarDecl(clang::VarDecl* var) { // !!!!!!!!!!!!!!!!!!!!!!!! пример !!!!!!!!!!!!!
    
    // беру правую часть (определение) переменной var
    clang::Expr* init = var->getInit();

    // если правой части нет - значит и нет malloc/new/fopen
    if (init == nullptr) {
      return true;
    }


    // выражения (в том числе malloc/new/fopen) могут быть в скобках
    // IgnoreParenImpCasts эти скобки игнорирует 
    // и отдает только само выражение
    clang::Expr* coreExpr = init->IgnoreParenCasts();

    // пытаюсь преврать выражение в вызов функции
    // (является ли coreExpr вызовом функции)
    clang::CallExpr* call = clang::dyn_cast<clang::CallExpr>(coreExpr);

    // если coreExpr - вызов функции
    if(call) {

      // получаем функцию, которая вызывается
      clang::FunctionDecl *func = call->getDirectCallee();

      // проверяем, что это не какой-нибудь странный вызов по указателю
      if (func) {

        
        // получаю имя функции
        llvm::StringRef funcName = func->getName();

        // если функция - malloc или fopen
          if (funcName == "malloc" || funcName == "fopen") {
            llvm::errs() << "Нашел malloc/fopen: " << var->getNameAsString() << "\n";
            // то для нашей переменной var вызывается malloc
            // переменную сохраняем
            m_allocatedVars.push_back(var);
          }
      }
    }
    // но new - это не функция, поэтому он отдельно обрабатывается

    clang::CXXNewExpr* is_new = clang::dyn_cast<clang::CXXNewExpr>(coreExpr);

    if(is_new) {
      llvm::errs() << "Нашел выделение (new): " << var->getNameAsString() << "\n";
      m_allocatedVars.push_back(var);
    }

    return true;
  } 




    void checkLeaks() {
      for (auto* allocVar : m_allocatedVars) {
          bool found = false;
          for (auto* deallocVar : m_deallocatedVars) {
              if (allocVar == deallocVar) {
                  found = true;
                  break;
              }
          }

          if (!found) {
              // Вот она, утечка!
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
  clang::ASTContext *m_context;
  std::vector<clang::VarDecl*> m_allocatedVars;
  std::vector<clang::VarDecl*> m_deallocatedVars;
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

    // ci.getASTContext() - передает информацию о типах 
    // и других деталях компиляции
    return std::make_unique<VotincevDConsumer>(&ci.getASTContext());
  }

  // позволяет плагину принимать аргументы из командной строки
  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    // но здесь просто возвращаем true и ничего не делаем
    return true;
  }
};
} // namespace


// регистрация плагина (чтобы его видел Clang)
static clang::FrontendPluginRegistry::Add<ExampleAction>
    X("votincev_d_analyzerplugin", "Description plugin");
