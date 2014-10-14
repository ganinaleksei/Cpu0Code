//===- lib/ReaderWriter/ELF/Cpu0/Cpu0LinkingContext.cpp ---------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Cpu0LinkingContext.h"

#include "lld/Core/File.h"
#include "lld/Core/Instrumentation.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringSwitch.h"

#include "Atoms.h"
#include "Cpu0RelocationPass.h"

using namespace lld;
using namespace lld::elf;

using llvm::makeArrayRef;

namespace {
using namespace llvm::ELF;

const uint8_t cpu0InitFiniAtomContent[8] = { 0 };

// Cpu0InitFini Atom
class Cpu0InitAtom : public InitFiniAtom {
public:
  Cpu0InitAtom(const File &f, StringRef function)
      : InitFiniAtom(f, ".init_array") {
#ifndef NDEBUG
    _name = "__init_fn_";
    _name += function;
#endif
  }
  ArrayRef<uint8_t> rawContent() const override {
    return makeArrayRef(cpu0InitFiniAtomContent);
  }
  Alignment alignment() const override { return Alignment(3); }
};

class Cpu0FiniAtom : public InitFiniAtom {
public:
  Cpu0FiniAtom(const File &f, StringRef function)
      : InitFiniAtom(f, ".fini_array") {
#ifndef NDEBUG
    _name = "__fini_fn_";
    _name += function;
#endif
  }
  ArrayRef<uint8_t> rawContent() const override {
    return makeArrayRef(cpu0InitFiniAtomContent);
  }

  Alignment alignment() const override { return Alignment(3); }
};

class Cpu0InitFiniFile : public SimpleFile {
public:
  Cpu0InitFiniFile(const ELFLinkingContext &context)
      : SimpleFile("command line option -init/-fini"), _ordinal(0) {}

  void addInitFunction(StringRef name) {
    Atom *initFunctionAtom = new (_allocator) SimpleUndefinedAtom(*this, name);
    Cpu0InitAtom *initAtom =
           (new (_allocator) Cpu0InitAtom(*this, name));
    initAtom->addReferenceELF_Cpu0(llvm::ELF::R_CPU0_32, 0,
                                     initFunctionAtom, 0);
    initAtom->setOrdinal(_ordinal++);
    addAtom(*initFunctionAtom);
    addAtom(*initAtom);
  }

  void addFiniFunction(StringRef name) {
    Atom *finiFunctionAtom = new (_allocator) SimpleUndefinedAtom(*this, name);
    Cpu0FiniAtom *finiAtom =
           (new (_allocator) Cpu0FiniAtom(*this, name));
    finiAtom->addReferenceELF_Cpu0(llvm::ELF::R_CPU0_32, 0,
                                     finiFunctionAtom, 0);
    finiAtom->setOrdinal(_ordinal++);
    addAtom(*finiFunctionAtom);
    addAtom(*finiAtom);
  }

private:
  llvm::BumpPtrAllocator _allocator;
  uint64_t _ordinal;
};

} // end anon namespace

void elf::Cpu0elLinkingContext::addPasses(PassManager &pm) {
  auto pass = createCpu0RelocationPass(*this);
  if (pass)
    pm.add(std::move(pass));
  ELFLinkingContext::addPasses(pm);
}

void elf::Cpu0elLinkingContext::createInternalFiles(
    std::vector<std::unique_ptr<File> > &result) const {
  ELFLinkingContext::createInternalFiles(result);
  std::unique_ptr<Cpu0InitFiniFile> initFiniFile(
      new Cpu0InitFiniFile(*this));
  for (auto ai : initFunctions())
    initFiniFile->addInitFunction(ai);
  for (auto ai:finiFunctions())
    initFiniFile->addFiniFunction(ai);
  result.push_back(std::move(initFiniFile));
}

