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

void Cpu0InstrInfo::
copyPhysReg(MachineBasicBlock &MBB,
            MachineBasicBlock::iterator I, DebugLoc DL,
            unsigned DestReg, unsigned SrcReg,
            bool KillSrc) const {
  unsigned Opc = 0, ZeroReg = 0;

  if (Cpu0::CPURegsRegClass.contains(DestReg)) { // Copy to CPU Reg.
    if (Cpu0::CPURegsRegClass.contains(SrcReg))
      Opc = Cpu0::ADD, ZeroReg = Cpu0::ZERO;
    else if (SrcReg == Cpu0::HI)
      Opc = Cpu0::MFHI, SrcReg = 0;
    else if (SrcReg == Cpu0::LO)
      Opc = Cpu0::MFLO, SrcReg = 0;
  }
  else if (Cpu0::CPURegsRegClass.contains(SrcReg)) { // Copy from CPU Reg.
    if (DestReg == Cpu0::HI)
      Opc = Cpu0::MTHI, DestReg = 0;
    else if (DestReg == Cpu0::LO)
      Opc = Cpu0::MTLO, DestReg = 0;
  }

  assert(Opc && "Cannot copy registers");

  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, get(Opc));

  if (DestReg)
    MIB.addReg(DestReg, RegState::Define);

  if (ZeroReg)
    MIB.addReg(ZeroReg);

  if (SrcReg)
    MIB.addReg(SrcReg, getKillRegState(KillSrc));
} // lbd document - mark - copyPhysReg

//- st SrcReg, MMO(FI)
void Cpu0InstrInfo::
storeRegToStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                    unsigned SrcReg, bool isKill, int FI,
                    const TargetRegisterClass *RC,
                    const TargetRegisterInfo *TRI) const {
} // lbd document - mark - storeRegToStackSlot

void Cpu0InstrInfo:: // lbd document - mark - before loadRegFromStackSlot
loadRegFromStackSlot(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                     unsigned DestReg, int FI,
                     const TargetRegisterClass *RC,
                     const TargetRegisterInfo *TRI) const
{
} // lbd document - mark - loadRegFromStackSlot

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
