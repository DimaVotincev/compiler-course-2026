#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace clang;

namespace {

struct VarState {
  bool ptrModified = false;
  bool dataModified = false;
};

class LocalVarConstVisitor : public RecursiveASTVisitor<LocalVarConstVisitor> {
public:
  LocalVarConstVisitor(ASTContext *ctx, Rewriter &rewriter)
      : m_ctx(ctx), m_rewriter(rewriter) {}

  Rewriter &getRewriter() { return m_rewriter; }

  // Обработка локальных переменных
  bool VisitVarDecl(VarDecl *var) {
    if (!var->isLocalVarDecl() || var->isStaticLocal())
      return true;

    auto *func = dyn_cast<FunctionDecl>(var->getDeclContext());
    if (!func || !func->hasBody())
      return true;

    if (isCandidate(var->getType()))
      m_candidates.insert(var);

    return true;
  }

  // Обработка параметров функций
  bool VisitParmVarDecl(ParmVarDecl *parm) {
    auto *func = dyn_cast<FunctionDecl>(parm->getDeclContext());
    if (!func || !func->hasBody())
      return true;

    if (isCandidate(parm->getType()))
      m_candidates.insert(parm);

    return true;
  }

  // Отслеживание присваиваний
  bool VisitBinaryOperator(BinaryOperator *op) {
    if (op->isAssignmentOp())
      markModified(op->getLHS(), false, true);
    return true;
  }

  // Отслеживание инкрементов/декрементов
  bool VisitUnaryOperator(UnaryOperator *op) {
    if (op->isIncrementDecrementOp()) {
      Expr *sub = op->getSubExpr()->IgnoreParenImpCasts();
      if (sub->getType()->isPointerType())
        markModified(sub, true, false);
      else
        markModified(sub, false, true);
    }
    return true;
  }

  // Применение замен ко всем кандидатам
  void applyChanges() {
    for (ValueDecl *decl : m_candidates) {
      VarState st = m_state[decl];
      QualType type = decl->getType();

      if (type->isReferenceType()) {
        if (!st.dataModified &&
            !type.getNonReferenceType().isConstQualified()) {
          QualType pointee = type.getNonReferenceType();
          std::string newType = "const " + pointee.getAsString() + "&";
          replaceType(decl, newType);
        }
      } else if (type->isPointerType()) {
        bool makePointeeConst =
            !st.dataModified && !type->getPointeeType().isConstQualified();
        bool makePtrConst = !st.ptrModified && !type.isConstQualified();

        if (makePointeeConst || makePtrConst) {
          std::string newType =
              buildPointerString(type, makePointeeConst, makePtrConst);
          replaceType(decl, newType);
        }
      }
    }
  }

private:
  ASTContext *m_ctx;
  Rewriter &m_rewriter;
  llvm::DenseSet<ValueDecl *> m_candidates;
  llvm::DenseMap<const ValueDecl *, VarState> m_state;

  // Проверка, можно ли добавить const к типу
  bool isCandidate(QualType type) {
    if (type->isReferenceType())
      return !type.getNonReferenceType().isConstQualified();
    if (type->isPointerType()) {
      const PointerType *ptr = type->getAs<PointerType>();
      return !type.isConstQualified() &&
             !ptr->getPointeeType().isConstQualified();
    }
    return false;
  }

  // Рекурсивно помечает переменные в выражении как модифицированные
  void markModified(Expr *E, bool isPtrMod, bool isDataMod) {
    if (!E)
      return;
    E = E->IgnoreParenImpCasts();

    if (auto *dre = dyn_cast<DeclRefExpr>(E)) {
      if (auto *decl = dyn_cast<ValueDecl>(dre->getDecl())) {
        VarState &st = m_state[decl];
        if (isPtrMod)
          st.ptrModified = true;
        if (isDataMod)
          st.dataModified = true;
      }
      return;
    }

    if (auto *uop = dyn_cast<UnaryOperator>(E)) {
      if (uop->getOpcode() == UO_Deref)
        markModified(uop->getSubExpr(), false, isDataMod);
      else
        markModified(uop->getSubExpr(), isPtrMod, isDataMod);
      return;
    }

    if (auto *arr = dyn_cast<ArraySubscriptExpr>(E)) {
      markModified(arr->getBase(), false, isDataMod);
      return;
    }

    if (auto *binop = dyn_cast<BinaryOperator>(E)) {
      markModified(binop->getLHS(), isPtrMod, isDataMod);
      markModified(binop->getRHS(), isPtrMod, isDataMod);
      return;
    }
  }

  // Формирует строку нового типа для указателя с нужными const
  std::string buildPointerString(QualType type, bool addConstToPointee,
                                 bool addConstToPtr) {
    std::vector<std::string> stars;
    QualType pointee = type;
    while (pointee->isPointerType()) {
      stars.push_back("*");
      pointee = pointee->getPointeeType();
    }
    std::string base = pointee.getAsString();
    while (!base.empty() && isspace(base.back()))
      base.pop_back();
    if (addConstToPointee)
      base = "const " + base;
    std::string result = base;
    for (const auto &s : stars) {
      result += s;
    }
    if (addConstToPtr)
      result += " const";
    return result;
  }

  // Заменяет старый тип новым в исходном коде
  void replaceType(ValueDecl *decl, const std::string &newType) {
    TypeSourceInfo *tsi = nullptr;
    if (auto *vd = dyn_cast<VarDecl>(decl))
      tsi = vd->getTypeSourceInfo();
    else if (auto *pd = dyn_cast<ParmVarDecl>(decl))
      tsi = pd->getTypeSourceInfo();

    if (!tsi)
      return;

    SourceRange range = tsi->getTypeLoc().getSourceRange();
    if (range.isValid())
      m_rewriter.ReplaceText(range, newType);
  }
};

class LocalVarConstConsumer : public ASTConsumer {
public:
  explicit LocalVarConstConsumer(ASTContext *ctx, Rewriter &rewriter)
      : m_visitor(ctx, rewriter) {}

  // Обход AST и применение изменений
  void HandleTranslationUnit(ASTContext &ctx) override {
    m_visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    m_visitor.applyChanges();

    m_visitor.getRewriter()
        .getEditBuffer(m_visitor.getRewriter().getSourceMgr().getMainFileID())
        .write(llvm::outs());
  }

private:
  LocalVarConstVisitor m_visitor;
};

class LocalVarConstAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    m_rewriter.setSourceMgr(ci.getSourceManager(), ci.getLangOpts());
    return std::make_unique<LocalVarConstConsumer>(&ci.getASTContext(),
                                                   m_rewriter);
  }
  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

private:
  Rewriter m_rewriter;
};

} // namespace

static FrontendPluginRegistry::Add<LocalVarConstAction>
    X("const_plugin",
      "Adds const to local pointers/references based on mutations");