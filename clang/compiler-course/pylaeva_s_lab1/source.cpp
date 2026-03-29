#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <map>
#include <set>
#include <string>

namespace {

struct Resources {
  clang::SourceLocation loc;
  std::string type;
  std::string varName;

  bool operator<(const Resources &other) const {
    if (loc.getRawEncoding() != other.loc.getRawEncoding())
      return loc.getRawEncoding() < other.loc.getRawEncoding();
    if (type != other.type)
      return type < other.type;
    return varName < other.varName;
  }
};

class PylaevaSVIsitor final
    : public clang::RecursiveASTVisitor<PylaevaSVIsitor> {
public:
  explicit PylaevaSVIsitor(clang::ASTContext *context,
                           clang::DiagnosticsEngine &diag)
      : m_context(context), m_diag(diag),
        m_sourceManager(context->getSourceManager()) {
    m_memDiag =
        m_diag.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                               "potential memory leak detected at line %0");
    m_fileDiag = m_diag.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "potential file handle leak detected at line %0");
  }

  // поиск вызовов функций
  bool VisitCallExpr(clang::CallExpr *call) {
    clang::FunctionDecl *func = call->getDirectCallee();
    if (!func)
      return true;

    std::string name = func->getNameInfo().getName().getAsString();
    clang::SourceLocation loc = call->getExprLoc();

    if (!m_sourceManager.isInMainFile(loc))
      return true;

    if (name == "malloc" || name == "calloc" || name == "realloc" ||
        name == "fopen") {
      std::string varName = "unknown";

      // Ищем присваивание или инициализацию с учетом приведения типов
      const clang::Stmt *currentStmt = call;
      auto parents = m_context->getParents(*currentStmt);

      while (!parents.empty()) {
        if (auto *castExpr = parents.begin()[0].get<clang::CStyleCastExpr>()) {
          currentStmt = castExpr;
          parents = m_context->getParents(*currentStmt);
          continue;
        }

        if (auto *binaryOp = parents.begin()[0].get<clang::BinaryOperator>()) {
          if (binaryOp->getOpcode() == clang::BO_Assign) {
            if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(
                    binaryOp->getLHS()->IgnoreImpCasts())) {
              if (auto *varDecl =
                      llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
                varName = varDecl->getNameAsString();
              }
            }
          }
          break;
        }

        // проверка на VarDecl (для инициализации при объявлении)
        if (auto *varDecl = parents.begin()[0].get<clang::VarDecl>()) {
          varName = varDecl->getNameAsString();
          break;
        }

        break;
      }

      if (name == "fopen") {
        m_resources.insert({loc, "file", varName});
      } else {
        m_resources.insert({loc, "memory", varName});
      }
    } else if (name == "free") {
      // При освобождении ресурса ищем по типу и области видимости
      std::string varName = "unknown";

      // Пытаемся найти имя переменной, которая освобождается
      if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(
              call->getArg(0)->IgnoreImpCasts())) {
        if (auto *varDecl =
                llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
          varName = varDecl->getNameAsString();
        }
      }

      auto it = m_resources.begin();
      while (it != m_resources.end()) {
        if (it->type == "memory" && it->varName == varName) {
          it = m_resources.erase(it);
          break;
        } else {
          ++it;
        }
      }
    } else if (name == "fclose") {
      std::string varName = "unknown";

      if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(
              call->getArg(0)->IgnoreImpCasts())) {
        if (auto *varDecl =
                llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
          varName = varDecl->getNameAsString();
        }
      }

      auto it = m_resources.begin();
      while (it != m_resources.end()) {
        if (it->type == "file" && it->varName == varName) {
          it = m_resources.erase(it);
          break;
        } else {
          ++it;
        }
      }
    }

    return true;
  }

  // поиск new
  bool VisitCXXNewExpr(clang::CXXNewExpr *newExpr) {
    clang::SourceLocation loc = newExpr->getExprLoc();
    if (!m_sourceManager.isInMainFile(loc))
      return true;

    std::string varName = "unknown";

    // Проверяем разные способы инициализации
    if (auto *parent = m_context->getParents(*newExpr)
                           .begin()[0]
                           .get<clang::BinaryOperator>()) {
      if (parent->getOpcode() == clang::BO_Assign) {
        if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(
                parent->getLHS()->IgnoreImpCasts())) {
          if (auto *varDecl =
                  llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
            varName = varDecl->getNameAsString();
          }
        }
      }
    } else if (auto *varDecl = m_context->getParents(*newExpr)
                                   .begin()[0]
                                   .get<clang::VarDecl>()) {
      varName = varDecl->getNameAsString();
    }

    m_resources.insert({loc, "memory", varName});
    return true;
  }

  // поиск delete
  bool VisitCXXDeleteExpr(clang::CXXDeleteExpr *deleteExpr) {
    // Получаем аргумент delete - указатель на удаляемый объект
    clang::Expr *arg = deleteExpr->getArgument()->IgnoreImpCasts();
    if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(arg)) {
      if (auto *varDecl = llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
        std::string varName = varDecl->getNameAsString();

        // Ищем ресурс с таким же именем переменной, типом "memory"
        auto it = m_resources.begin();
        while (it != m_resources.end()) {
          if (it->type == "memory" && it->varName == varName) {
            it = m_resources.erase(it);
            break;
          } else {
            ++it;
          }
        }
      }
    }
    return true;
  }

  void reportLeaks() {
    for (const auto &res : m_resources) {
      if (m_sourceManager.isInMainFile(res.loc)) {
        unsigned lineNum = m_sourceManager.getSpellingLineNumber(res.loc);

        if (res.type == "memory") {
          m_diag.Report(res.loc, m_memDiag) << lineNum;
        } else if (res.type == "file") {
          m_diag.Report(res.loc, m_fileDiag) << lineNum;
        }
      }
    }
  }

private:
  clang::ASTContext *m_context;
  clang::DiagnosticsEngine &m_diag;
  clang::SourceManager &m_sourceManager;
  std::multiset<Resources> m_resources;
  unsigned m_memDiag;
  unsigned m_fileDiag;
};

class PylaevaSConsumer final : public clang::ASTConsumer {
public:
  explicit PylaevaSConsumer(clang::ASTContext *context,
                            clang::DiagnosticsEngine &diag)
      : m_visitor(context, diag) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.reportLeaks();
  }

private:
  PylaevaSVIsitor m_visitor;
};

class PylaevaSAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<PylaevaSConsumer>(&ci.getASTContext(),
                                              ci.getDiagnostics());
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static clang::FrontendPluginRegistry::Add<PylaevaSAction>
    X("pylaeva_s_lab1_plugin", "Detects resource leaks");