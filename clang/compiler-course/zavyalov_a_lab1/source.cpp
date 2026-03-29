#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace {
class ZavyalovVisitor final
    : public clang::RecursiveASTVisitor<ZavyalovVisitor> {
public:
  explicit ZavyalovVisitor(clang::ASTContext *context,
                           clang::Rewriter &rewriter)
      : m_rewriter(rewriter) {}

  bool VisitVarDecl(clang::VarDecl *varDecl) {

    clang::QualType varType = varDecl->getType();

    if (varType.isConstQualified()) {
      return true;
    }

    if (varType->isReferenceType()) {
      if (varType.getNonReferenceType().isConstQualified()) {
        return true; // пропускаем const ссылку
      }
    }

    if (varType->isPointerType()) {
      const_ptrs.insert(varDecl);

      clang::Expr *initExpr = varDecl->getInit();
      if (initExpr) {
        // проверка что указатель инициализируется с помощью взятия адреса
        if (auto *op = llvm::dyn_cast<clang::UnaryOperator>(initExpr)) {
          if (op->getOpcode() == clang::UO_AddrOf) {
            clang::Expr *subExpr = op->getSubExpr()->IgnoreParenImpCasts();

            if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(subExpr)) {
              if (auto *rhs_varDecl =
                      llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
                aliasGraph[varDecl].push_back(rhs_varDecl);
                aliasGraph[rhs_varDecl].push_back(varDecl);
              }
            }
          }
        }

        // проверка что указатель инициализируется другим указателем
        if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(
                initExpr->IgnoreParenImpCasts())) {
          if (auto *rhs_varDecl =
                  llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
            if (rhs_varDecl->getType()->isPointerType()) {
              aliasGraph[varDecl].push_back(rhs_varDecl);
              aliasGraph[rhs_varDecl].push_back(varDecl);
            }
          }
        }
      }
    }

    if (varType->isReferenceType()) {
      const_refs.insert(varDecl);

      clang::Expr *initExpr = varDecl->getInit();

      // проверка что ссылка инициализируется другой ссылкой
      if (initExpr)
        if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(initExpr))
          if (auto *rhs_varDecl =
                  llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
            if (rhs_varDecl->getType()->isReferenceType()) {
              aliasGraph[rhs_varDecl].push_back(varDecl);
              aliasGraph[varDecl].push_back(rhs_varDecl);
            }
          }
    }
    return true;
  }

  bool VisitBinaryOperator(clang::BinaryOperator *op) {
    if (op->isAssignmentOp()) { // CompoundAssignmentOp is included in
                                // AssignmentOp

      clang::Expr *lhsExpr = op->getLHS()->IgnoreParenImpCasts();

      // является ли lhs разыменованием ptr
      if (auto *unaryOp = llvm::dyn_cast<clang::UnaryOperator>(lhsExpr)) {
        if (unaryOp->getOpcode() == clang::UO_Deref) {
          clang::Expr *subExpr = unaryOp->getSubExpr()->IgnoreParenCasts();
          if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(subExpr)) {
            if (auto *varDecl =
                    llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
              if (const_ptrs.find(varDecl) != const_ptrs.end()) {
                const_ptrs.erase(const_ptrs.find(varDecl));
              }
            }
          }
        }
      }

      if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(lhsExpr)) {
        if (auto *varDecl =
                llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {

          // проверка является ли lhs ссылкой
          if (varDecl->getType()->isReferenceType()) {
            if (const_refs.find(varDecl) != const_refs.end()) {
              const_refs.erase(const_refs.find(varDecl));
            }
          }

          // проверка является ли lhs указателем
          if (varDecl->getType()->isPointerType()) {
            if (const_ptrs.find(varDecl) != const_ptrs.end()) {
              const_ptrs.erase(const_ptrs.find(varDecl));
            }
          }
        }
      }
    }

    return true;
  }

  bool VisitUnaryOperator(clang::UnaryOperator *op) {

    if (op->getOpcode() == clang::UO_PreInc ||
        op->getOpcode() == clang::UO_PostInc ||
        op->getOpcode() == clang::UO_PreDec ||
        op->getOpcode() == clang::UO_PostDec) {
      clang::Expr *subExpr = op->getSubExpr()->IgnoreParenImpCasts();

      if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(subExpr)) {
        if (auto *referenceVar =
                llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
          if (const_refs.find(referenceVar) != const_refs.end()) {
            const_refs.erase(const_refs.find(referenceVar));
          }
          if (const_ptrs.find(referenceVar) != const_ptrs.end()) {
            const_ptrs.erase(const_ptrs.find(referenceVar));
          }
        }
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr *call) {
    size_t args_count = call->getNumArgs();
    clang::FunctionDecl *decl =
        llvm::dyn_cast<clang::FunctionDecl>(call->getCalleeDecl());
    if (!decl)
      return true;

    for (size_t i = 0; i < args_count && i < decl->getNumParams(); i++) {
      clang::Expr *arg = call->getArg(i)->IgnoreParenImpCasts();

      clang::ParmVarDecl *paramDecl = decl->getParamDecl(i);

      // проверка что в функцию передается ссылка неконстантно
      if (paramDecl->getType()->isReferenceType()) {
        bool isConstRef =
            paramDecl->getType().getNonReferenceType().isConstQualified();
        if (!isConstRef) {
          if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(arg)) {
            if (auto *referenceVar =
                    llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
              if (const_refs.find(referenceVar) != const_refs.end()) {
                const_refs.erase(const_refs.find(referenceVar));
              }
            }
          }
        }
      }

      // проверка что в неконстантную функцию передается адрес
      if (paramDecl->getType()->isPointerType()) {
        bool isConstRef =
            paramDecl->getType()->getPointeeType().isConstQualified();
        if (!isConstRef) {
          if (auto *op = llvm::dyn_cast<clang::UnaryOperator>(arg)) {
            if (op->getOpcode() ==
                clang::UO_AddrOf) { // в функцию передаётся адрес взятый через &
              clang::Expr *subExpr = op->getSubExpr()->IgnoreParenImpCasts();
              if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(subExpr)) {
                if (auto *referenceVar =
                        llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {

                  if (const_refs.find(referenceVar) != const_refs.end()) {
                    const_refs.erase(const_refs.find(referenceVar));
                  }
                }
              }
            }
          }
        }
      }
    }
    return true;
  }

  bool VisitCXXMemberCallExpr(clang::CXXMemberCallExpr *call) {
    clang::Expr *objectExpr =
        call->getImplicitObjectArgument()->IgnoreParenImpCasts();
    if (auto *declRef = llvm::dyn_cast<clang::DeclRefExpr>(objectExpr)) {
      if (auto *varDecl = llvm::dyn_cast<clang::VarDecl>(declRef->getDecl())) {
        if (!(call->getMethodDecl()->isConst())) {

          if (const_refs.find(varDecl) != const_refs.end()) {
            const_refs.erase(const_refs.find(varDecl));
          }
          if (const_ptrs.find(varDecl) != const_ptrs.end()) {
            const_ptrs.erase(const_ptrs.find(varDecl));
          }
        }
      }
    }
    return true;
  }

  void endOfFile() {
    removePointeesFromSets();

    for (clang::VarDecl *varDecl : const_ptrs) {
      clang::SourceLocation typeStart = varDecl->getTypeSpecStartLoc();
      clang::SourceLocation typeEnd = varDecl->getTypeSpecEndLoc();

      if (typeStart.isValid() && typeEnd.isValid()) {

        m_rewriter.ReplaceText(clang::SourceRange(typeStart, typeEnd),
                               "const " + varDecl->getType().getAsString() +
                                   " const ");
      }
    }
    for (clang::VarDecl *varDecl : const_refs) {
      clang::SourceLocation typeStart = varDecl->getTypeSpecStartLoc();
      clang::SourceLocation typeEnd = varDecl->getTypeSpecEndLoc();

      if (typeStart.isValid() && typeEnd.isValid()) {

        m_rewriter.ReplaceText(clang::SourceRange(typeStart, typeEnd),
                               "const " + varDecl->getType().getAsString());
      }
    }
  }

private:
  std::set<clang::VarDecl *> const_ptrs;
  std::set<clang::VarDecl *> const_refs;
  std::map<clang::VarDecl *, std::vector<clang::VarDecl *>>
      aliasGraph; // мапа из varDecl, где varDecl - это поинтер/ссылка, в вектор
                  // поинтеров/ссылок, которые будут меняться вместе с
                  // изменением этого varDecl
  clang::Rewriter &m_rewriter;

  void removePointeesByPtr(clang::VarDecl *ptr,
                           std::set<clang::VarDecl *> &visited) {
    const std::vector<clang::VarDecl *> &pointees = aliasGraph[ptr];
    for (const auto &ptr : pointees) {
      if (visited.find(ptr) != visited.end())
        continue;

      visited.insert(ptr);
      if (const_ptrs.find(ptr) != const_ptrs.end()) {
        const_ptrs.erase(const_ptrs.find(ptr));
      } else if (const_refs.find(ptr) != const_refs.end()) {
        const_refs.erase(const_refs.find(ptr));
      }
      removePointeesByPtr(ptr, visited);
    }
  }

  void removePointeesFromSets() {
    std::set<clang::VarDecl *> visited;
    for (const auto &p : aliasGraph) {
      clang::VarDecl *ptr_or_reference = p.first;
      if ((const_ptrs.find(ptr_or_reference) == const_ptrs.end()) &&
          (const_refs.find(ptr_or_reference) ==
           const_refs.end())) { // this ptr/ref is non-const so every other node
                                // needs to be removed
        removePointeesByPtr(ptr_or_reference, visited);
      }
    }
  }
};

class ZavyalovConsumer final : public clang::ASTConsumer {
public:
  explicit ZavyalovConsumer(clang::ASTContext *context,
                            clang::Rewriter &rewriter)
      : m_visitor(context, rewriter), m_rewriter(rewriter) {}

  void HandleTranslationUnit(clang::ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_visitor.endOfFile();
  }

private:
  ZavyalovVisitor m_visitor;
  clang::Rewriter &m_rewriter;
};

class ZavyalovAction final : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<ZavyalovConsumer>(&ci.getASTContext(), m_rewriter);
  }

  bool ParseArgs(const clang::CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }

  void EndSourceFileAction() override {
    auto &buffer =
        m_rewriter.getEditBuffer(m_rewriter.getSourceMgr().getMainFileID());

    std::string content;
    llvm::raw_string_ostream stream(content);
    buffer.write(stream);

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
      if (line.find("//") == std::string::npos) {
        llvm::outs() << line << "\n";
      }
    }
  }

private:
  clang::Rewriter m_rewriter;
};
} // namespace

static clang::FrontendPluginRegistry::Add<ZavyalovAction>
    X("zavyalov_a_lab1_plugin", "Plugin for replacing non-const pointers and "
                                "references with const ones whenever possible");
