//===-- Cpu0TargetObjectFile.cpp - Cpu0 Object Files ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Cpu0TargetObjectFile.h"
#include "Cpu0Subtarget.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ELF.h"
using namespace llvm;

static cl::opt<unsigned>
SSThreshold("cpu0-ssection-threshold", cl::Hidden,
            cl::desc("Small data and bss section threshold size (default=8)"),
            cl::init(8));

void Cpu0TargetObjectFile::Initialize(MCContext &Ctx, const TargetMachine &TM){
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);

  SmallDataSection =
    getContext().getELFSection(".sdata", ELF::SHT_PROGBITS,
                               ELF::SHF_WRITE |ELF::SHF_ALLOC,
                               SectionKind::getDataRel());

  SmallBSSSection =
    getContext().getELFSection(".sbss", ELF::SHT_NOBITS,
                               ELF::SHF_WRITE |ELF::SHF_ALLOC,
                               SectionKind::getBSS());

}

