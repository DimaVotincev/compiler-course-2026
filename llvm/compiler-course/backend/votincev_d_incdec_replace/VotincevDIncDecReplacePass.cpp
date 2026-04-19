#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include <vector>

using namespace llvm;

namespace {
class VotincevDIncDecReplacePass : public MachineFunctionPass {
public:
  static char ID;
  VotincevDIncDecReplacePass() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
};

char VotincevDIncDecReplacePass::ID = 0;

bool VotincevDIncDecReplacePass::runOnMachineFunction(MachineFunction &func) {
  const X86Subtarget &STI = func.getSubtarget<X86Subtarget>();
  const TargetInstrInfo *TII = STI.getInstrInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  MachineRegisterInfo &MRI = func.getRegInfo();
  bool Changed = false;

  for (MachineBasicBlock &MBB : func) {
    SmallPtrSet<MachineInstr *, 16> ToEraseSet;
    std::vector<MachineInstr *> ToEraseList;

    for (MachineInstr &MI : MBB) {
      // пропускаем инструкции, которые уже включены в цепочку и подлежат
      // удалению
      if (ToEraseSet.count(&MI))
        continue;

      unsigned Opcode = MI.getOpcode();

      if (Opcode == X86::INC32r || Opcode == X86::DEC32r) {
        MachineInstr *Root = &MI;
        Register OriginalSrc = MI.getOperand(1).getReg();
        Register LastDest = MI.getOperand(0).getReg();
        int TotalOffset = (Opcode == X86::INC32r) ? 1 : -1;

        std::vector<MachineInstr *> ToDelete;
        MachineInstr *Current = Root;

        // поиск цепочки
        while (true) {
          Register NextReg = Current->getOperand(0).getReg();
          if (!MRI.hasOneNonDBGUse(NextReg))
            break;

          MachineInstr &NextMI = *MRI.use_nodbg_instructions(NextReg).begin();

          if (NextMI.getParent() != &MBB)
            break;

          unsigned NextOpc = NextMI.getOpcode();
          if (NextOpc == X86::INC32r || NextOpc == X86::DEC32r) {
            if (NextMI.readsRegister(X86::EFLAGS, TRI))
              break;

            TotalOffset += (NextOpc == X86::INC32r) ? 1 : -1;
            LastDest = NextMI.getOperand(0).getReg();
            ToDelete.push_back(&NextMI);
            Current = &NextMI;
          } else {
            break;
          }
        }

        // замена INC/DEC на ADD/SUB
        if (!ToDelete.empty()) {
          MachineInstr &LastInst = ToDelete.empty() ? *Root : *ToDelete.back();
          DebugLoc DL = LastInst.getDebugLoc();

          if (TotalOffset > 0) {
            MachineInstrBuilder MIB =
                BuildMI(MBB, Root, DL, TII->get(X86::ADD32ri), LastDest)
                    .addReg(OriginalSrc)
                    .addImm(TotalOffset);

            MIB.copyImplicitOps(LastInst);
          } else if (TotalOffset < 0) {
            MachineInstrBuilder MIB =
                BuildMI(MBB, Root, DL, TII->get(X86::SUB32ri), LastDest)
                    .addReg(OriginalSrc)
                    .addImm(-TotalOffset);

            MIB.copyImplicitOps(LastInst);
          } else { // COPY чтобы регистр не затерся
            BuildMI(MBB, Root, DL, TII->get(X86::COPY), LastDest)
                .addReg(OriginalSrc);
          }

          ToEraseSet.insert(Root);
          ToEraseList.push_back(Root);
          for (MachineInstr *Instr : ToDelete) {
            ToEraseSet.insert(Instr);
            ToEraseList.push_back(Instr);
          }
          Changed = true;
        }
      }
    }

    for (MachineInstr *Instr : ToEraseList) {
      Instr->eraseFromParent();
    }
  }
  return Changed;
}
} // namespace

static RegisterPass<VotincevDIncDecReplacePass>
    X("votincev_d_incdec_replace-x86", "description pass", false, false);