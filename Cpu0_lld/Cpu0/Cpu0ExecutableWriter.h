//===- lib/ReaderWriter/ELF/Cpu0/Cpu0ExecutableWriter.h ------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef Cpu0_EXECUTABLE_WRITER_H
#define Cpu0_EXECUTABLE_WRITER_H

#include "ExecutableWriter.h"
#include "Cpu0LinkingContext.h"

namespace lld {
namespace elf {

template <class ELFT>
class Cpu0ExecutableWriter : public ExecutableWriter<ELFT> {
public:
  Cpu0ExecutableWriter(Cpu0LinkingContext &context,
                         Cpu0TargetLayout<ELFT> &layout);

protected:
  // Add any runtime files and their atoms to the output
  virtual bool createImplicitFiles(std::vector<std::unique_ptr<File>> &);

  virtual void finalizeDefaultAtomValues() {
    return ExecutableWriter<ELFT>::finalizeDefaultAtomValues();
  }

  virtual void addDefaultAtoms() {
    return ExecutableWriter<ELFT>::addDefaultAtoms();
  }

private:
  class GOTFile : public SimpleFile {
  public:
    GOTFile(const ELFLinkingContext &eti) : SimpleFile("GOTFile") {}
    llvm::BumpPtrAllocator _alloc;
  };

  std::unique_ptr<GOTFile> _gotFile;
  Cpu0LinkingContext &_context;
  Cpu0TargetLayout<ELFT> &_cpu0Layout;
};

template <class ELFT>
Cpu0ExecutableWriter<ELFT>::Cpu0ExecutableWriter(
    Cpu0LinkingContext &context, Cpu0TargetLayout<ELFT> &layout)
    : ExecutableWriter<ELFT>(context, layout), _gotFile(new GOTFile(context)),
      _context(context), _cpu0Layout(layout) {}

template <class ELFT>
bool Cpu0ExecutableWriter<ELFT>::createImplicitFiles(
    std::vector<std::unique_ptr<File>> &result) {
  ExecutableWriter<ELFT>::createImplicitFiles(result);
  _gotFile->addAtom(*new (_gotFile->_alloc) GLOBAL_OFFSET_TABLEAtom(*_gotFile));
  _gotFile->addAtom(*new (_gotFile->_alloc) TLSGETADDRAtom(*_gotFile));
  if (_context.isDynamic())
    _gotFile->addAtom(*new (_gotFile->_alloc) DYNAMICAtom(*_gotFile));
  result.push_back(std::move(_gotFile));
  return true;
}

} // namespace elf
} // namespace lld

#endif
