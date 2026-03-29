#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {
struct VotincedDFremPass : llvm::PassInfoMixin<VotincedDFremPass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    // frem -> a-(a/b)*b  for fp and int

    bool changed = false;
    for (auto &bb : func) {
      for (auto &instr : llvm::make_early_inc_range(bb)) {
        if (auto *binOp = llvm::dyn_cast<llvm::BinaryOperator>(&instr)) {

          if (binOp->getOpcode() == llvm::Instruction::FRem) {
            binOp->dump();
            auto *lhs = binOp->getOperand(0);
            auto *rhs = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *op1 =
                builder.CreateFDiv(lhs, rhs); // a/b (обычное деление fp)
            op1 = builder.CreateUnaryIntrinsic(llvm::Intrinsic::trunc,
                                               op1);     // округление a/b
            auto *op2 = builder.CreateFMul(op1, rhs);    // (a/b) * b
            auto *fremOp = builder.CreateFSub(lhs, op2); // a - ( (a/b)*b )

            binOp->replaceAllUsesWith(fremOp);
            binOp->eraseFromParent();
            changed = true;

          } else if (binOp->getOpcode() == llvm::Instruction::URem) {
            binOp->dump();
            auto *lhs = binOp->getOperand(0);
            auto *rhs = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *op1 = builder.CreateUDiv(
                lhs, rhs); // a/b (обычное целочисленное деление fp)
            auto *op2 = builder.CreateMul(op1, rhs);    // (a/b) * b
            auto *uremOp = builder.CreateSub(lhs, op2); // a - ( (a/b)*b )

            binOp->replaceAllUsesWith(uremOp);
            binOp->eraseFromParent();
            changed = true;
          } else if (binOp->getOpcode() == llvm::Instruction::SRem) {
            binOp->dump();
            auto *lhs = binOp->getOperand(0);
            auto *rhs = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *op1 = builder.CreateSDiv(
                lhs, rhs); // a/b (обычное целочисленное деление fp)
            auto *op2 = builder.CreateMul(op1, rhs);    // (a/b) * b
            auto *sremOp = builder.CreateSub(lhs, op2); // a - ( (a/b)*b )

            binOp->replaceAllUsesWith(sremOp);
            binOp->eraseFromParent();
            changed = true;
          }
        }
      }
    }

    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  } // run

  static bool isRequired() { return true; }
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "VotincedDFremPass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "votincev_d_frem") {
                    FPM.addPass(VotincedDFremPass{});
                    return true;
                  }
                  return false;
                });
          }};
}
