//===---------------------------- elf2hex.cpp -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This program is a utility that works with llvm-objdump.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#define DLINK
//#define ELF2HEX_DEBUG

static cl::opt<bool>
ConvertElf2Hex("elf2hex", 
cl::desc("Display the hex content of verilog cpu0 needed sections"));

static cl::opt<bool>
LittleEndian("le", 
cl::desc("Little endian format"));

#ifdef DLINK
  static cl::opt<bool>
  DumpSo("cpu0dumpso", 
  cl::desc("Dump shared library .so"));
  
  static cl::opt<bool>
  LinkSo("cpu0linkso", 
  cl::desc("Link shared library .so"));
  
  #include "elf2hex-dlinker.h"
#endif

// Modified from PrintSectionHeaders()
static uint64_t GetSectionHeaderStartAddress(const ObjectFile *Obj, 
  StringRef sectionName) {
//  outs() << "Sections:\n"
//            "Idx Name          Size      Address          Type\n";
  std::error_code ec;
  unsigned i = 0;
  for (const SectionRef &Section : Obj->sections()) {
    if (error(ec)) return 0;
    StringRef Name;
    if (error(Section.getName(Name))) return 0;
    uint64_t Address;
    if (error(Section.getAddress(Address))) return 0;
    uint64_t Size;
    if (error(Section.getSize(Size))) return 0;
    bool Text, Data, BSS;
    if (error(Section.isText(Text))) return 0;
    if (error(Section.isData(Data))) return 0;
    if (error(Section.isBSS(BSS))) return 0;
    if (Name == sectionName)
      return Address;
    else
      return 0;
    ++i;
  }
  return 0;
}

// Fill /*address*/ 00 00 00 00 [startAddr..endAddr] from startAddr to endAddr. 
// Include startAddr and endAddr.
static void Fill0s(uint64_t startAddr, uint64_t endAddr) {
  std::size_t addr;

  assert((startAddr <= endAddr) && "startAddr must <= BaseAddr");
  // Fill /*address*/ bytes is odd for 4 by 00 
  outs() << format("/*%8" PRIx64 " */", startAddr);
  // Fill /*address*/ 00 00 00 00 for 4 bytes alignment (1 Cpu0 word size)
  for (addr = startAddr; addr < endAddr; addr += 4) {
    outs() << format("/*%8" PRIx64 " */", addr);
    outs() << format("%02" PRIx64 " ", 0) << format("%02" PRIx64 " ", 0) \
    << format("%02" PRIx64 " ", 0) << format("%02" PRIx64 " ", 0) << '\n';
  }

  return;
}

static void PrintDataSection(const ObjectFile *o, uint64_t& lastDumpAddr, 
  SectionRef Section) {
  std::string Error;
  std::size_t addr, end;
  StringRef Name;
  StringRef Contents;
  uint64_t BaseAddr;
  bool BSS;
  uint64_t size;
  if (error(Section.getName(Name))) return;
  if (error(Section.getContents(Contents))) return;
  if (error(Section.getAddress(BaseAddr))) return;
  if (error(Section.isBSS(BSS))) return;

  size = (Contents.size()+3)/4*4;
  if (Contents.size() <= 0) {
    return;
  }

  outs() << "/*Contents of section " << Name << ":*/\n";
  // Dump out the content as hex and printable ascii characters.
  for (std::size_t addr = 0, end = Contents.size(); addr < end; addr += 16) {
    outs() << format("/*%8" PRIx64 " */", BaseAddr + addr);
    // Dump line of hex.
    for (std::size_t i = 0; i < 16; ++i) {
      if (i != 0 && i % 4 == 0)
        outs() << ' ';
      if (addr + i < end)
        outs() << hexdigit((Contents[addr + i] >> 4) & 0xF, true)
               << hexdigit(Contents[addr + i] & 0xF, true) << " ";
    }
    // Print ascii.
    outs() << "/*" << "  ";
    for (std::size_t i = 0; i < 16 && addr + i < end; ++i) {
      if (std::isprint(static_cast<unsigned char>(Contents[addr + i]) & 0xFF))
        outs() << Contents[addr + i];
      else
        outs() << ".";
    }
    outs() << "*/" << "\n";
  }
  for (std::size_t i = Contents.size(); i < size; i++) {
    outs() << "00 ";
  }
  outs() << "\n";
#ifdef ELF2HEX_DEBUG
  errs() << "Name " << Name << "  BaseAddr ";
  errs() << format("%8" PRIx64 " Contents.size() ", BaseAddr);
  errs() << format("%8" PRIx64 " size ", Contents.size());
  errs() << format("%8" PRIx64 " \n", size);
#endif
  // save the end address of this section to lastDumpAddr
  lastDumpAddr = BaseAddr + size;
}

// Modified from DisassembleObject()
static void DisassembleObjectInHexFormat(const ObjectFile *Obj
/*, bool InlineRelocs*/  , std::unique_ptr<MCDisassembler>& DisAsm, 
  std::unique_ptr<MCInstPrinter>& IP, uint64_t& lastDumpAddr) {
  std::string Error;
  uint64_t soLastPrintAddr = 0;
  FILE *fd_so_func_offset;
  int num_dyn_entry = 0;

#ifdef ELF2HEX_DEBUG
  errs() << format("!lastDumpAddr %8" PRIx64 "\n", lastDumpAddr);
#endif
  std::error_code ec;
  for (const SectionRef &Section : Obj->sections()) {
    if (error(ec)) break;
    StringRef Name;
    StringRef Contents;
    uint64_t BaseAddr;
    if (error(Section.getName(Name))) continue;
    if (error(Section.getContents(Contents))) continue;
    if (error(Section.getAddress(BaseAddr))) continue;
    if (BaseAddr < 0x100)
      continue;
  #ifdef ELF2HEX_DEBUG
    errs() << "Name " << Name << format("  BaseAddr %8" PRIx64 "\n", BaseAddr);
    errs() << format("!!lastDumpAddr %8" PRIx64 "\n", lastDumpAddr);
  #endif
    bool text;
    if (error(Section.isText(text))) break;
    if (!text) {
    #ifdef ELF2HEX_DEBUG
      errs() << "!text\n";
    #endif
      if (lastDumpAddr < BaseAddr) {
        Fill0s(lastDumpAddr, BaseAddr - 1);
        lastDumpAddr = BaseAddr;
      }
      if (Name == ".got.plt") {
        uint64_t BaseAddr;
        if (error(Section.getAddress(BaseAddr))) 
          assert(1 && "Cannot get BaseAddr of section .got.plt");
      #ifdef DLINK
        if (LinkSo) {
          raw_fd_ostream fd_global_offset("dlconfig/global_offset", Error, 
                                          sys::fs::F_Text);
          fd_global_offset << format("%02" PRIx64 " ", BaseAddr >> 24);
          fd_global_offset << format("%02" PRIx64 " ", (BaseAddr >> 16) & 0xFF);
          fd_global_offset << format("%02" PRIx64 " ", (BaseAddr >> 8) & 0xFF);
          fd_global_offset << format("%02" PRIx64 "    ", BaseAddr & 0xFF);
        }
      #endif
        PrintDataSection(Obj, lastDumpAddr, Section);
      }
      else if ((Name == ".bss" || Name == ".sbss") && Contents.size() > 0) {
        uint64_t size = (Contents.size() + 3)/4*4;
        Fill0s(BaseAddr, BaseAddr + size - 1);
        lastDumpAddr = BaseAddr + size;
        continue;
      }
      else {
        PrintDataSection(Obj, lastDumpAddr, Section);
      }
      continue;
    }
    else {
      if (lastDumpAddr < BaseAddr) {
        Fill0s(lastDumpAddr, BaseAddr - 1);
        lastDumpAddr = BaseAddr;
      }
    }
    // It's .text section
    uint64_t SectionAddr;
    if (error(Section.getAddress(SectionAddr))) break;

    // Make a list of all the symbols in this section.
    std::vector<std::pair<uint64_t, StringRef> > Symbols;
    for (const SymbolRef &Symbol : Obj->symbols()) {
      bool contains;
      if (!error(Section.containsSymbol(Symbol, contains)) && contains) {
        uint64_t Address;
        if (error(Symbol.getAddress(Address))) break;
        if (Address == UnknownAddressOrSize) continue;
        Address -= SectionAddr;

        StringRef Name;
        if (error(Symbol.getName(Name))) break;
        Symbols.push_back(std::make_pair(Address, Name));
      }
    }

    // Sort the symbols by address, just in case they didn't come in that way.
    array_pod_sort(Symbols.begin(), Symbols.end());

    // Make a list of all the relocations for this section.
    std::vector<RelocationRef> Rels;

    // Sort relocations by address.
    std::sort(Rels.begin(), Rels.end(), RelocAddressLess);

    StringRef SegmentName = "";
    if (const MachOObjectFile *MachO =
        dyn_cast<const MachOObjectFile>(Obj)) {
      DataRefImpl DR = Section.getRawDataRefImpl();
      SegmentName = MachO->getSectionFinalSegmentName(DR);
    }
    StringRef name;
    if (error(Section.getName(name))) break;
    outs() << "/*" << "Disassembly of section ";
    if (!SegmentName.empty())
      outs() << SegmentName << ",";
    outs() << name << ':' << "*/";

    // If the section has no symbols just insert a dummy one and disassemble
    // the whole section.
    if (Symbols.empty())
      Symbols.push_back(std::make_pair(0, name));

    SmallString<40> Comments;
    raw_svector_ostream CommentStream(Comments);

    StringRef Bytes;
    if (error(Section.getContents(Bytes))) break;
    StringRefMemoryObject memoryObject(Bytes, SectionAddr);
    uint64_t Size;
    uint64_t Index;
    uint64_t SectSize;
    if (error(Section.getSize(SectSize))) break;

    std::vector<RelocationRef>::const_iterator rel_cur = Rels.begin();
    std::vector<RelocationRef>::const_iterator rel_end = Rels.end();
    // Disassemble symbol by symbol.
    for (unsigned si = 0, se = Symbols.size(); si != se; ++si) {
      uint64_t Start = Symbols[si].first;
      uint64_t End;
      // The end is either the size of the section or the beginning of the next
      // symbol.
      if (si == se - 1)
        End = SectSize;
      // Make sure this symbol takes up space.
      else if (Symbols[si + 1].first != Start)
        End = Symbols[si + 1].first - 1;
      else {
        outs() << '\n' << "/*" << Symbols[si].second << ":*/\n";
        continue;
      }

      outs() << '\n' << "/*" << Symbols[si].second << ":*/\n";
      uint16_t funIndex = 0;
    #ifdef DLINK
      if (LinkSo) {
      // correctDynFunIndex
        funIndex = cpu0DynFunIndex.correctDynFunIndex(Symbols[si].second.data());
      }
    #endif

#ifndef NDEBUG
        raw_ostream &DebugOut = DebugFlag ? dbgs() : nulls();
#else
        raw_ostream &DebugOut = nulls();
#endif

      for (Index = Start; Index < End; Index += Size) {
        MCInst Inst;

    #ifdef DLINK
        if (LinkSo && funIndex && Index == Start) {
          outs() << format("/*%8" PRIx64 ":*/\t", SectionAddr + Index);
          outs() << "01 6b " << format("%02" PRIx64, (funIndex*4+16) & 0xff00)
                  << format(" %02" PRIx64, (funIndex*4+16) & 0x00ff);
          outs() << "                                  /* ld\t$t9, " 
                 << funIndex*4+16 << "($gp)\n";
        }
        else
    #endif
       {
          if (DisAsm->getInstruction(Inst, Size, memoryObject,
                                     SectionAddr + Index,
                                     DebugOut, CommentStream)) {
            outs() << format("/*%8" PRIx64 ":*/", SectionAddr + Index);
            if (!NoShowRawInsn) {
              outs() << "\t";
              DumpBytes(StringRef(Bytes.data() + Index, Size));
            }
            outs() << "/*";
            IP->printInst(&Inst, outs(), "");
            outs() << CommentStream.str();
            outs() << "*/";
            Comments.clear();
            outs() << "\n";
          } else {
            errs() << ToolName << ": warning: invalid instruction encoding\n";
            if (Size == 0)
              Size = 1; // skip illegible bytes
          }
        }

        //  outs() << "Size = " << Size <<  "Index = " << Index << "lastDumpAddr = "
        //         << lastDumpAddr << "\n"; // debug
        // Print relocation for instruction.
        while (rel_cur != rel_end) {
          bool hidden = false;
          uint64_t addr;
          SmallString<16> name;
          SmallString<32> val;

          // If this relocation is hidden, skip it.
          if (error(rel_cur->getHidden(hidden))) goto skip_print_rel;
          if (hidden) goto skip_print_rel;

          if (error(rel_cur->getOffset(addr))) goto skip_print_rel;
          // Stop when rel_cur's address is past the current instruction.
          if (addr >= Index + Size) break;
          if (error(rel_cur->getTypeName(name))) goto skip_print_rel;
          if (error(rel_cur->getValueString(val))) goto skip_print_rel;

          outs() << format("\t\t\t/*%8" PRIx64 ": ", SectionAddr + addr) << name
                 << "\t" << val << "*/\n";

        skip_print_rel:
          ++rel_cur;
        }
      }
    #ifdef ELF2HEX_DEBUG
      errs() << format("SectionAddr + Index = %8" PRIx64 "\n", SectionAddr + Index);
      errs() << format("lastDumpAddr %8" PRIx64 "\n", lastDumpAddr);
    #endif
    }
    // In section .plt or .text, the Contents.size() maybe < (SectionAddr + Index)
    if (Contents.size() < (SectionAddr + Index))
      lastDumpAddr = SectionAddr + Index;
    else
      lastDumpAddr = SectionAddr + Contents.size();
  }
}

static uint64_t SectionOffset(const ObjectFile *o, StringRef secName) {
  std::error_code ec;

  for (const SectionRef &Section : o->sections()) {
    if (error(ec)) return 0;
    StringRef Name;
    StringRef Contents;
    uint64_t BaseAddr;
    bool BSS;
    if (error(Section.getName(Name))) return 0;
    if (error(Section.getContents(Contents))) return 0;
    if (error(Section.getAddress(BaseAddr))) return 0;
    if (error(Section.isBSS(BSS))) return 0;

    if (Name == secName)
      return BaseAddr;
  }
  return 0;
}

static void PrintBootSection(uint64_t pltOffset, bool isLittleEndian) {
  uint64_t offset = pltOffset - 4;
  if (isLittleEndian) {
    outs() << "/*       0:*/	";
    outs() << format("%02" PRIx64 " ", (offset & 0xff));
    outs() << format("%02" PRIx64 "", (offset & 0xff00) >> 8);
    outs() << " 00 36";
    outs() << "                                  /*	jmp	0x";
    outs() << format("%02" PRIx64 "%02" PRIx64 " */\n", (offset & 0xff00) >> 8, 
                     (offset & 0xff)); 
    outs() <<
      "/*       4:*/	04 00 00 36                                  /*	jmp	4 */\n" <<
      "/*       8:*/	04 00 00 36                                  /*	jmp	4 */\n" <<
      "/*       c:*/	fc ff ff 36                                  /*	jmp	-4 */\n";
  }
  else {
    outs() << "/*       0:*/	36 00 ";
    outs() << format("%02" PRIx64 " ", (offset & 0xff00) >> 8);
    outs() << format("%02" PRIx64 "", (offset & 0xff));
    outs() << "                                  /*	jmp	0x";
    outs() << format("%02" PRIx64 "%02" PRIx64 " */\n", (offset & 0xff00) >> 8, 
                     (offset & 0xff)); 
    outs() <<
      "/*       4:*/	36 00 00 04                                  /*	jmp	4 */\n" <<
      "/*       8:*/	36 00 00 04                                  /*	jmp	4 */\n" <<
      "/*       c:*/	36 ff ff fc                                  /*	jmp	-4 */\n";
  }
}

static void Elf2Hex(const ObjectFile *o) {
  uint64_t lastDumpAddr = 0;

  const Target *TheTarget = getTarget(o);
  // getTarget() will have already issued a diagnostic if necessary, so
  // just bail here if it failed.
  if (!TheTarget)
    return;

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    SubtargetFeatures Features;
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  std::unique_ptr<const MCRegisterInfo> MRI(TheTarget->createMCRegInfo(TripleName));
  if (!MRI) {
    errs() << "error: no register info for target " << TripleName << "\n";
    return;
  }

  // Set up disassembler.
  std::unique_ptr<const MCAsmInfo> AsmInfo(
    TheTarget->createMCAsmInfo(*MRI, TripleName));
  if (!AsmInfo) {
    errs() << "error: no assembly info for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCSubtargetInfo> STI(
    TheTarget->createMCSubtargetInfo(TripleName, "", FeaturesStr));
  if (!STI) {
    errs() << "error: no subtarget info for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCInstrInfo> MII(TheTarget->createMCInstrInfo());
  if (!MII) {
    errs() << "error: no instruction info for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCObjectFileInfo> MOFI(new MCObjectFileInfo);
  MCContext Ctx(AsmInfo.get(), MRI.get(), MOFI.get());

  std::unique_ptr<MCDisassembler> DisAsm(
    TheTarget->createMCDisassembler(*STI, Ctx));
  if (!DisAsm) {
    errs() << "error: no disassembler for target " << TripleName << "\n";
    return;
  }

  if (Symbolize) {
    std::unique_ptr<MCRelocationInfo> RelInfo(
        TheTarget->createMCRelocationInfo(TripleName, Ctx));
    if (RelInfo) {
      std::unique_ptr<MCSymbolizer> Symzer(
        MCObjectSymbolizer::createObjectSymbolizer(Ctx, std::move(RelInfo),
                                                   o));
      if (Symzer)
        DisAsm->setSymbolizer(std::move(Symzer));
    }
  }

  std::unique_ptr<const MCInstrAnalysis> MIA(
      TheTarget->createMCInstrAnalysis(MII.get()));

  int AsmPrinterVariant = AsmInfo->getAssemblerDialect();
  std::unique_ptr<MCInstPrinter> IP(TheTarget->createMCInstPrinter(
      AsmPrinterVariant, *AsmInfo, *MII, *MRI, *STI));
  if (!IP) {
    errs() << "error: no instruction printer for target " << TripleName
      << '\n';
    return;
  }

  if (CFG) {
    std::unique_ptr<MCObjectDisassembler> OD(
      new MCObjectDisassembler(*o, *DisAsm, *MIA));
    std::unique_ptr<MCModule> Mod(OD->buildModule(/* withCFG */ true));
    for (MCModule::const_atom_iterator AI = Mod->atom_begin(),
                                       AE = Mod->atom_end();
                                       AI != AE; ++AI) {
      outs() << "Atom " << (*AI)->getName() << ": \n";
      if (const MCTextAtom *TA = dyn_cast<MCTextAtom>(*AI)) {
        for (MCTextAtom::const_iterator II = TA->begin(), IE = TA->end();
             II != IE;
             ++II) {
          IP->printInst(&II->Inst, outs(), "");
          outs() << "\n";
        }
      }
    }
    for (MCModule::const_func_iterator FI = Mod->func_begin(),
                                       FE = Mod->func_end();
                                       FI != FE; ++FI) {
      static int filenum = 0;
      emitDOTFile((Twine((*FI)->getName()) + "_" +
                   utostr(filenum) + ".dot").str().c_str(),
                    **FI, IP.get());
      ++filenum;
    }
  }

  uint64_t startAddr = GetSectionHeaderStartAddress(o, "_start");
//  outs() << format("_start address:%08" PRIx64 "\n", startAddr);
#ifdef DLINK
  if (DumpSo) {
    DisassembleSoInHexFormat(o, DisAsm, IP, lastDumpAddr);
    PrintSoDataSections(o, lastDumpAddr, LittleEndian);
  }
  else
#endif
  {
    std::string Error;
    uint64_t pltOffset = SectionOffset(o, ".plt");
    PrintBootSection(pltOffset, LittleEndian);
  #ifdef DLINK
    if (LinkSo) {
      cpu0DynFunIndex.createPltName(o);
      cpu0DynFunIndex.createStrtab();
      raw_fd_ostream fd_plt_offset("dlconfig/plt_offset", Error, 
                                    sys::fs::F_Text);
      fd_plt_offset << format("%08" PRIx64 " ", pltOffset);
    }
  #endif
    lastDumpAddr = 16;
    Fill0s(lastDumpAddr, 0x100);
    lastDumpAddr = 0x100;
    DisassembleObjectInHexFormat(o, DisAsm, IP, lastDumpAddr);
  }
}
