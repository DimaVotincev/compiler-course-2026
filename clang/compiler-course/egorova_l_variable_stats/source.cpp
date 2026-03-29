#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>

using namespace clang;

struct VariableStats {
  int global = 0;
  int static_global = 0;
  int static_local = 0;
  int local = 0;
  int parameter = 0;

  int total_functions = 0;
  std::string current_file;

  void print() {
    auto &out = llvm::outs();
    out << "\n=== Variable Statistics for Translation Unit ===\n";
    if (!current_file.empty()) {
      out << "File: " << current_file << "\n";
    }
    out << "Global variables:        " << global << "\n";
    out << "Static global variables: " << static_global << "\n";
    out << "Static local variables:  " << static_local << "\n";
    out << "Local variables:         " << local << "\n";
    out << "Function parameters:     " << parameter << "\n";
    out << "==============================================\n";
    out << "TOTAL:                   "
        << (global + static_global + static_local + local + parameter) << "\n";
    if (total_functions > 0) {
      out << "Functions analyzed:      " << total_functions << "\n";
    }
    out << "\n";
  }
};

class EgorovaVariableStatsVisitor final
    : public RecursiveASTVisitor<EgorovaVariableStatsVisitor> {
public:
  EgorovaVariableStatsVisitor(ASTContext *context, VariableStats &stats)
      : m_context(context), m_stats(stats) {

    SourceManager &sm = context->getSourceManager();
    SourceLocation mainFileLoc = sm.getLocForStartOfFile(sm.getMainFileID());
    if (sm.isInMainFile(mainFileLoc)) {
      StringRef filename = sm.getFilename(mainFileLoc);
      if (!filename.empty()) {
        m_stats.current_file = filename.str();
      }
    }
  }

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (FD->isThisDeclarationADefinition() && isInMainFile(FD->getLocation())) {
      m_stats.total_functions++;
    }
    return true;
  }

  bool VisitVarDecl(VarDecl *VD) {
    if (VD->isImplicit() || VD->getLocation().isInvalid() ||
        !isInMainFile(VD->getLocation())) {
      return true;
    }

    if (isa<ParmVarDecl>(VD)) {
      return true;
    }

    if (!VD->isThisDeclarationADefinition()) {
      return true;
    }

    if (!VD->isFileVarDecl()) {
      if (VD->isStaticLocal()) {
        m_stats.static_local++;
      } else {
        m_stats.local++;
      }
    } else {
      if (VD->getStorageClass() == SC_Static) {
        m_stats.static_global++;
      } else {
        m_stats.global++;
      }
    }
    return true;
  }

  bool VisitParmVarDecl(ParmVarDecl *PD) {
    if (!isInMainFile(PD->getLocation())) {
      return true;
    }
    if (auto *FD = dyn_cast<FunctionDecl>(PD->getDeclContext())) {
      if (!FD->isThisDeclarationADefinition()) {
        return true;
      }
    }

    m_stats.parameter++;
    return true;
  }

private:
  bool isInMainFile(SourceLocation Loc) {
    if (Loc.isInvalid())
      return false;
    return m_context->getSourceManager().isInMainFile(Loc);
  }

  ASTContext *m_context;
  VariableStats &m_stats;
};

class EgorovaVariableStatsConsumer final : public ASTConsumer {
public:
  explicit EgorovaVariableStatsConsumer(ASTContext *context)
      : m_visitor(context, m_stats) {}

  void HandleTranslationUnit(ASTContext &context) override {
    m_visitor.TraverseDecl(context.getTranslationUnitDecl());
    m_stats.print();
  }

private:
  VariableStats m_stats;
  EgorovaVariableStatsVisitor m_visitor;
};

class EgorovaVariableStatsAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<EgorovaVariableStatsConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

static FrontendPluginRegistry::Add<EgorovaVariableStatsAction>
    X("egorova-variable-stats",
      "Plugin for collecting statistics about variables");