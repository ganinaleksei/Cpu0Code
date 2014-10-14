//===- lib/ReaderWriter/ELF/Cpu0/Cpu0TargetHandler.h ------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_READER_WRITER_ELF_Cpu0_Cpu0_TARGET_HANDLER_H
#define LLD_READER_WRITER_ELF_Cpu0_Cpu0_TARGET_HANDLER_H

#include "DefaultTargetHandler.h"
#include "ELFFile.h"
#include "Cpu0RelocationHandler.h"
#include "TargetLayout.h"

#include "lld/Core/Simple.h"

#define DLINKER

namespace lld {
namespace elf {
typedef llvm::object::ELFType<llvm::support::big, 2, false> Cpu0ELFType;
class Cpu0LinkingContext;

template <class ELFT> class Cpu0TargetLayout : public TargetLayout<ELFT> {
public:
  Cpu0TargetLayout(Cpu0LinkingContext &context)
      : TargetLayout<ELFT>(context) {}
};

class Cpu0TargetHandler final
    : public DefaultTargetHandler<Cpu0ELFType> {
public:
  Cpu0TargetHandler(Cpu0LinkingContext &context);

  Cpu0TargetLayout<Cpu0ELFType> &getTargetLayout() override {
    return *(_cpu0TargetLayout.get());
  }

  void registerRelocationNames(Registry &registry) override;

  const Cpu0TargetRelocationHandler &getRelocationHandler() const override {
    return *(_cpu0RelocationHandler.get());
  }

  std::unique_ptr<Writer> getWriter() override;

private:
  static const Registry::KindStrings kindStrings[];
  Cpu0LinkingContext &_context;
  std::unique_ptr<Cpu0TargetLayout<Cpu0ELFType>> _cpu0TargetLayout;
  std::unique_ptr<Cpu0TargetRelocationHandler> _cpu0RelocationHandler;
};

} // end namespace elf
} // end namespace lld

#endif
