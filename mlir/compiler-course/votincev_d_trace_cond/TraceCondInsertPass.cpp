#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

class TraceCondPass
    : public PassWrapper<TraceCondPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "votincev_d_trace_cond_MLIR"; }
  StringRef getDescription() const final {
    return "Inserts trace function calls at the begin and end of then/else "
           "blocks.";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();
    MLIRContext *ctx = moduleOp.getContext();
    OpBuilder builder(ctx);

    // Вспомогательная функция для генерации объявлений функций трассировки
    auto ensureFuncDeclared = [&](StringRef name) {
      if (!moduleOp.lookupSymbol<func::FuncOp>(name)) {
        builder.setInsertionPointToStart(moduleOp.getBody());
        auto funcType = builder.getFunctionType({}, {});
        builder.create<func::FuncOp>(builder.getUnknownLoc(), name, funcType)
            .setPrivate();
      }
    };

    ensureFuncDeclared("trace_condition_then_begin");
    ensureFuncDeclared("trace_condition_then_end");
    ensureFuncDeclared("trace_condition_else_begin");
    ensureFuncDeclared("trace_condition_else_end");

    // Функция для вставки call-операций в начало и конец блока
    auto insertCalls = [&](Block &block, StringRef beginName,
                           StringRef endName) {
      if (block.empty())
        return;

      // Вставка в начало блока
      builder.setInsertionPointToStart(&block);
      builder.create<func::CallOp>(builder.getUnknownLoc(), beginName,
                                   TypeRange{});

      // Вставка в конец блока (перед yield/return)
      Operation &terminator = block.back();
      builder.setInsertionPoint(&terminator);
      builder.create<func::CallOp>(builder.getUnknownLoc(), endName,
                                   TypeRange{});
    };

    // Обход всех операций
    moduleOp.walk([&](Operation *op) {
      if (auto scfIf = dyn_cast<scf::IfOp>(op)) {
        insertCalls(*scfIf.thenBlock(), "trace_condition_then_begin",
                    "trace_condition_then_end");
        if (scfIf.elseBlock()) {
          insertCalls(*scfIf.elseBlock(), "trace_condition_else_begin",
                      "trace_condition_else_end");
        }
      } else if (auto affineIf = dyn_cast<affine::AffineIfOp>(op)) {
        insertCalls(*affineIf.getThenBlock(), "trace_condition_then_begin",
                    "trace_condition_then_end");
        if (affineIf.hasElse()) {
          insertCalls(*affineIf.getElseBlock(), "trace_condition_else_begin",
                      "trace_condition_else_end");
        }
      }
    });
  }
};

} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TraceCondPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TraceCondPass)

mlir::PassPluginLibraryInfo getTraceCondPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TraceCondPass", "1.0",
          []() { mlir::PassRegistration<TraceCondPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTraceCondPassPluginInfo();
}