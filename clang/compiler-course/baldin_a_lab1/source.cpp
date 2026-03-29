#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>

using namespace clang;

namespace {

// Состояние переменной
struct VarStatus {
  bool pointerMutated = false;
  bool dataMutated = false;
};

class MutationCollector : public RecursiveASTVisitor<MutationCollector> {
public:
  const std::unordered_map<const VarDecl *, VarStatus> &getMap() {
    return m_statusMap;
  }

  // Ловим присваивания: = , += , -= и тд
  bool VisitBinaryOperator(BinaryOperator *BO) {
    if (BO->isAssignmentOp()) {
      checkExpression(BO->getLHS(), false);
    }
    return true;
  }

  // Ловим инкременты/декременты: ++ , --
  bool VisitUnaryOperator(UnaryOperator *UO) {
    if (UO->isIncrementDecrementOp()) {
      checkExpression(UO->getSubExpr(), false);
    }
    return true;
  }

private:
  std::unordered_map<const VarDecl *, VarStatus> m_statusMap;

  // Анализирует выражение слева от `=` или внутри `++`
  void checkExpression(Expr *E, bool isDeref) {
    if (!E)
      return;
    E = E->IgnoreParenCasts();

    // Базовый случай: дошли до самой переменной
    if (auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      if (auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        if (isDeref)
          m_statusMap[VD].dataMutated = true;
        else
          m_statusMap[VD].pointerMutated = true;
      }
      return;
    }

    // Разыменование: *p
    if (auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() == UO_Deref) {
        checkExpression(UO->getSubExpr(), true);
      }
      return;
    }

    // Доступ по индексу массива: p[i]
    if (auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      checkExpression(ASE->getBase(), true);
      return;
    }

    // Адресная арифметика: *(p + 1) или *(1 + p)
    if (auto *BO = dyn_cast<BinaryOperator>(E)) {
      checkExpression(BO->getLHS(), isDeref);
      checkExpression(BO->getRHS(), isDeref);
      return;
    }
  }
};

class ConstFixVisitor final : public RecursiveASTVisitor<ConstFixVisitor> {
public:
  explicit ConstFixVisitor(
      ASTContext *context, Rewriter &R,
      const std::unordered_map<const VarDecl *, VarStatus> &map)
      : m_context(context), m_rewriter(R), m_statusMap(map) {}

  bool VisitVarDecl(VarDecl *v) {
    QualType qt = v->getType();
    if (qt.isNull())
      return true;

    bool isPtr = qt->isPointerType();
    bool isRef = qt->isReferenceType();
    if (!isPtr && !isRef)
      return true;

    auto *func = dyn_cast<FunctionDecl>(v->getDeclContext());
    if (!func || !func->hasBody())
      return true;

    VarStatus status;

    auto it = m_statusMap.find(v);
    if (it != m_statusMap.end()) {
      status = it->second;
    }

    bool ptrMutated = status.pointerMutated;
    bool dataMutated = status.dataMutated;

    bool dataIsConst = qt->getPointeeType().isConstQualified();
    bool ptrIsConst = qt.isLocalConstQualified();

    if (isRef) {
      if (!dataMutated && !ptrMutated &&
          !qt.getNonReferenceType().isConstQualified()) {
        m_rewriter.InsertText(v->getBeginLoc(), "const ");
      }
    } else if (isPtr) {
      if (!ptrMutated && !dataMutated) {
        if (!dataIsConst)
          m_rewriter.InsertText(v->getBeginLoc(), "const ");
        if (!ptrIsConst)
          m_rewriter.InsertText(v->getLocation(), "const ");
      } else if (!dataMutated && ptrMutated) {
        if (!dataIsConst)
          m_rewriter.InsertText(v->getBeginLoc(), "const ");
      } else if (dataMutated && !ptrMutated) {
        if (!ptrIsConst)
          m_rewriter.InsertText(v->getLocation(), "const ");
      }
    }

    return true;
  }

private:
  ASTContext *m_context;
  Rewriter &m_rewriter;
  const std::unordered_map<const VarDecl *, VarStatus> &m_statusMap;
};

class ConstFixConsumer final : public ASTConsumer {
public:
  explicit ConstFixConsumer(CompilerInstance &CI) {}

  void HandleTranslationUnit(ASTContext &context) override {
    Rewriter rewriter;
    rewriter.setSourceMgr(context.getSourceManager(), context.getLangOpts());

    // Собираем информацию о переменных
    MutationCollector collector;
    collector.TraverseDecl(context.getTranslationUnitDecl());

    // Изменяем код
    ConstFixVisitor visitor(&context, rewriter, collector.getMap());
    visitor.TraverseDecl(context.getTranslationUnitDecl());

    // Вывод
    FileID mainFileID = context.getSourceManager().getMainFileID();
    const llvm::RewriteBuffer *RewriteBuf =
        rewriter.getRewriteBufferFor(mainFileID);

    if (RewriteBuf) {
      llvm::outs() << std::string(RewriteBuf->begin(), RewriteBuf->end());
    } else {
      bool invalid = false;
      StringRef buf =
          context.getSourceManager().getBufferData(mainFileID, &invalid);
      if (!invalid) {
        llvm::outs() << buf;
      }
    }

    llvm::outs().flush();
  }
};

class ConstFixaAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<ConstFixConsumer>(ci);
  }

  bool ParseArgs(const CompilerInstance &ci,
                 const std::vector<std::string> &args) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ConstFixaAction>
    X("const_plugin", "Adds const to pointers/refs");