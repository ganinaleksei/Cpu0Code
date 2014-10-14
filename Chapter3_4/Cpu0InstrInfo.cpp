//===-- Cpu0InstrInfo.cpp - Cpu0 Instruction Information ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Cpu0 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "Cpu0InstrInfo.h"
#include "Cpu0TargetMachine.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#define GET_INSTRINFO_CTOR_DTOR
#include "Cpu0GenInstrInfo.inc"

using namespace llvm;

Cpu0InstrInfo::Cpu0InstrInfo(Cpu0TargetMachine &tm)
  : 
    TM(tm),
    RI(*TM.getSubtargetImpl(), *this) {}

const Cpu0RegisterInfo &Cpu0InstrInfo::getRegisterInfo() const {
  return RI;
} // lbd document - mark - getRegisterInfo()

MachineInstr*
Cpu0InstrInfo::emitFrameIndexDebugValue(MachineFunction &MF, int FrameIx,
                                        uint64_t Offset, const MDNode *MDPtr,
                                        DebugLoc DL) const {
  MachineInstrBuilder MIB = BuildMI(MF, DL, get(Cpu0::DBG_VALUE))
    .addFrameIndex(FrameIx).addImm(0).addImm(Offset).addMetadata(MDPtr);
  return &*MIB;
} // lbd document - mark - emitFrameIndexDebugValue

// Cpu0InstrInfo::expandPostRAPseudo
/// Expand Pseudo instructions into real backend instructions
bool Cpu0InstrInfo::expandPostRAPseudo(MachineBasicBlock::iterator MI) const {
  MachineBasicBlock &MBB = *MI->getParent();

  switch(MI->getDesc().getOpcode()) {
  default:
    return false;
  case Cpu0::RetLR:
    ExpandRetLR(MBB, MI, Cpu0::RET);
    break;
  }

  MBB.erase(MI);
  return true;
}

void Cpu0InstrInfo::ExpandRetLR(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator I,
                                unsigned Opc) const {
  BuildMI(MBB, I, I->getDebugLoc(), get(Opc)).addReg(Cpu0::LR);
}
