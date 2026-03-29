#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

namespace {

class ThrowAndCallVisitor
    : public clang::RecursiveASTVisitor<ThrowAndCallVisitor> {
public:
  bool hasThrow = false;
  std::vector<const clang::FunctionDecl *> calls;

  bool VisitCXXThrowExpr(clang::CXXThrowExpr *) {
    hasThrow = true;
    return false;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    if (auto callee = call->getDirectCallee()) {
      calls.push_back(callee->getCanonicalDecl());
    } else {
      hasThrow = true;
    }
    return true; // всегда возвращаем true для продолжения обхода
  }

  // Обработка new-выражений (могут бросить std::bad_alloc)
  bool VisitCXXNewExpr(clang::CXXNewExpr *) {
    hasThrow = true;
    return false;
  }

  bool VisitCXXConstructExpr(clang::CXXConstructExpr *ctor) {
    if (auto constructor = ctor->getConstructor()) {
      calls.push_back(constructor->getCanonicalDecl());
    }
    return true;
  }
};

class SavvaDariyaConsumer final : public clang::ASTConsumer {
public:
  explicit SavvaDariyaConsumer(clang::ASTContext *context)
      : m_context(context) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {

    auto TU = context.getTranslationUnitDecl();

    std::vector<clang::FunctionDecl *> functions;

    for (auto decl : TU->decls()) {
      if (auto func = llvm::dyn_cast<clang::FunctionDecl>(decl)) {

        // Фильтруем только подходящие функции
        if (!func->hasBody())
          continue; // Нужно тело
        if (func->isImplicit())
          continue; // Неявные пропускаем
        if (func->isDeleted() || func->isDefaulted())
          continue; // Специальные
        if (context.getSourceManager().isInSystemHeader(func->getLocation()))
          continue;
        if (!func->getType()->isFunctionProtoType())
          continue; // Нужен прототип

        functions.push_back(func->getCanonicalDecl());
      }
    }

    // собираем throw и строим граф вызовов
    std::unordered_map<const clang::FunctionDecl *, bool> functionThrows;
    std::unordered_map<const clang::FunctionDecl *,
                       std::vector<const clang::FunctionDecl *>>
        callGraph;

    for (auto func : functions) {
      ThrowAndCallVisitor visitor;
      visitor.TraverseStmt(func->getBody());

      // Сохраняем результаты
      functionThrows[func] = visitor.hasThrow;
      callGraph[func] = visitor.calls;
    }

    // Распространяем информацию о throw по графу вызовов

    bool changed = true;
    while (changed) {
      changed = false;

      for (auto &entry : callGraph) {
        auto caller = entry.first;

        if (functionThrows[caller])
          continue;

        // Проверяем всех, кого вызывает эта функция
        for (auto callee : entry.second) {
          if (functionThrows[callee]) {
            functionThrows[caller] = true;
            changed = true;
            break;
          }
        }
      }
    }

    // Вставляем noexcept
    for (auto func : functions) {
      // Пропускаем функции, которые могут бросить исключение
      if (functionThrows[func])
        continue;

      // Пропускаем функции, уже имеющие noexcept
      if (func->getExceptionSpecType() == clang::EST_BasicNoexcept)
        continue;

      // Вставляем noexcept
      insertNoexcept(func);
    }

    TU->dump(llvm::outs());
  }

private:
  // Вспомогательный метод для вставки noexcept (без изменений)
  void insertNoexcept(clang::FunctionDecl *func) {
    const auto *ftp = func->getType()->getAs<clang::FunctionProtoType>();
    if (!ftp)
      return;
    clang::FunctionProtoType::ExtProtoInfo epi = ftp->getExtProtoInfo();
    epi.ExceptionSpec.Type = clang::EST_BasicNoexcept;

    clang::QualType newType = m_context->getFunctionType(
        ftp->getReturnType(), ftp->getParamTypes(), epi);
    func->setType(newType);
  }
  clang::ASTContext *m_context;
};

class SavvaDariyaAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<SavvaDariyaConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<SavvaDariyaAction>
    X("savva_dariya_3823b1fi1", "Add noexcept to non-throwing functions");
