//===-- Cpu0Subtarget.cpp - Cpu0 Subtarget Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Cpu0 specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "Cpu0Subtarget.h"
#include "Cpu0.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "cpu0-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "Cpu0GenSubtargetInfo.inc"

void Cpu0Subtarget::anchor() { }

Cpu0Subtarget::Cpu0Subtarget(const std::string &TT, const std::string &CPU,
                             const std::string &FS, bool little, 
                             Reloc::Model _RM) :
  Cpu0GenSubtargetInfo(TT, CPU, FS),
  Cpu0ABI(UnknownABI), IsLittle(little), RM(_RM)
{
  std::string CPUName = CPU;
  if (CPUName.empty())
    CPUName = "cpu032II";
  if (CPUName == "cpu032I")
    Cpu0ArchVersion = Cpu032I;
  else if (CPUName == "cpu032II")
    Cpu0ArchVersion = Cpu032II;

  if (hasCpu032I()) {
    HasCmp = true;
    HasSlt = false;
  }
  if (hasCpu032II())
    HasSlt = true;

  // Parse features string.
  ParseSubtargetFeatures(CPUName, FS);

  // Initialize scheduling itinerary for the specified CPU.
  InstrItins = getInstrItineraryForCPU(CPUName);

  // Set Cpu0ABI if it hasn't been set yet.
  if (Cpu0ABI == UnknownABI)
    Cpu0ABI = O32;
}

