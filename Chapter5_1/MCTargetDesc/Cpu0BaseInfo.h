//===-- Cpu0BaseInfo.h - Top level definitions for CPU0 MC ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone helper functions and enum definitions for
// the Cpu0 target useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//
#ifndef CPU0BASEINFO_H
#define CPU0BASEINFO_H

#include "Cpu0FixupKinds.h"
#include "Cpu0MCTargetDesc.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

/// Cpu0II - This namespace holds all of the target specific flags that
/// instruction info tracks.
///
namespace Cpu0II {
  /// Target Operand Flag enum.
  enum TOF {
    //===------------------------------------------------------------------===//
    // Cpu0 Specific MachineOperand flags.

    MO_NO_FLAG,
  }; // enum TOF {

  enum {
    //===------------------------------------------------------------------===//
    // Instruction encodings.  These are the standard/most common forms for
    // Cpu0 instructions.
    //

    // Pseudo - This represents an instruction that is a pseudo instruction
    // or one that has not been implemented yet.  It is illegal to code generate
    // it, but tolerated for intermediate implementation stages.
    Pseudo   = 0,

    /// FrmR - This form is for instructions of the format R.
    FrmR  = 1,
    /// FrmI - This form is for instructions of the format I.
    FrmI  = 2,
    /// FrmJ - This form is for instructions of the format J.
    FrmJ  = 3,
    /// FrmOther - This form is for instructions that have no specific format.
    FrmOther = 4,

    FormMask = 15
  };
}

/// getCpu0RegisterNumbering - Given the enum value for some register,
/// return the number that it corresponds to.
inline static unsigned getCpu0RegisterNumbering(unsigned RegEnum)
{
  switch (RegEnum) {
  case Cpu0::ZERO:
    return 0;
  case Cpu0::AT:
    return 1;
  case Cpu0::V0:
    return 2;
  case Cpu0::V1:
    return 3;
  case Cpu0::A0:
    return 4;
  case Cpu0::A1:
    return 5;
  case Cpu0::T9:
    return 6;
  case Cpu0::T0:
    return 7;
  case Cpu0::S0:
    return 8;
  case Cpu0::S1:
    return 9;
  case Cpu0::SW:
    return 10;
  case Cpu0::GP:
    return 11;
  case Cpu0::FP:
    return 12;
  case Cpu0::SP:
    return 13;
  case Cpu0::LR:
    return 14;
  case Cpu0::PC:
    return 15;
  case Cpu0::HI:
    return 18;
  case Cpu0::LO:
    return 19;
  default: llvm_unreachable("Unknown register number!");
  }
} // lbd document - mark - getCpu0RegisterNumbering

inline static std::pair<const MCSymbolRefExpr*, int64_t>
Cpu0GetSymAndOffset(const MCFixup &Fixup) {
  MCFixupKind FixupKind = Fixup.getKind();

  if ((FixupKind < FirstTargetFixupKind) ||
      (FixupKind >= MCFixupKind(Cpu0::LastTargetFixupKind)))
    return std::make_pair((const MCSymbolRefExpr*)0, (int64_t)0);

  const MCExpr *Expr = Fixup.getValue();
  MCExpr::ExprKind Kind = Expr->getKind();

  if (Kind == MCExpr::Binary) {
    const MCBinaryExpr *BE = static_cast<const MCBinaryExpr*>(Expr);
    const MCExpr *LHS = BE->getLHS();
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(BE->getRHS());

    if ((LHS->getKind() != MCExpr::SymbolRef) || !CE)
      return std::make_pair((const MCSymbolRefExpr*)0, (int64_t)0);

    return std::make_pair(cast<MCSymbolRefExpr>(LHS), CE->getValue());
  }

  if (Kind != MCExpr::SymbolRef)
    return std::make_pair((const MCSymbolRefExpr*)0, (int64_t)0);

  return std::make_pair(cast<MCSymbolRefExpr>(Expr), 0);
} // Cpu0GetSymAndOffset
} // lbd document - mark - namespace llvm - end

#endif
