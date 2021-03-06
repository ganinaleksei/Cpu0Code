#!/usr/bin/env bash
OS=`uname -s`
echo "OS =" ${OS}

if [ "$OS" == "Linux" ]; then
  TOOLDIR=/usr/local/llvm/test/cmake_debug_build/bin
else
  TOOLDIR=~/llvm/test/cmake_debug_build/Debug/bin
fi

if [ $1 != cpu032I ] && [ $1 != cpu032II ]; then
  echo "1st argument is cpu032I (default) or cpu032II"
  exit 1
fi

CPU=$1
echo "CPU =" "${CPU}"

bash rminput.sh
rm -rf dlconfig
mkdir dlconfig

clang -target mips-unknown-linux-gnu -c start.cpp -emit-llvm -o start.bc
clang -target mips-unknown-linux-gnu -c dynamic_linker.cpp -emit-llvm \
-o dynamic_linker.cpu0.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg-def.c -emit-llvm \
-o printf-stdarg-def.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg.c -emit-llvm \
-o printf-stdarg.bc
clang -target mips-unknown-linux-gnu -c foobar.cpp -emit-llvm -o foobar.cpu0.bc
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
-cpu0-reserve-gp=true dynamic_linker.cpu0.bc -o dynamic_linker.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
printf-stdarg-def.bc -o printf-stdarg-def.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
-cpu0-reserve-gp=true printf-stdarg.bc -o printf-stdarg.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=pic -filetype=obj \
-cpu0-reserve-gp=true -cpu0-no-cpload=true foobar.cpu0.bc -o foobar.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
lib_cpu0.ll -o lib_cpu0.o
${TOOLDIR}/lld -flavor gnu -target cpu0-unknown-linux-gnu -shared -o \
libfoobar.cpu0.so foobar.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
-cpu0-reserve-gp=true start.bc -o start.cpu0.o
clang -target mips-unknown-linux-gnu -c ch_dynamiclinker.cpp -emit-llvm \
-o ch_dynamiclinker.cpu0.bc
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
-cpu0-reserve-gp=true ch_dynamiclinker.cpu0.bc -o ch_dynamiclinker.cpu0.o
${TOOLDIR}/lld -flavor gnu -target cpu0-unknown-linux-gnu start.cpu0.o \
printf-stdarg-def.cpu0.o printf-stdarg.cpu0.o dynamic_linker.cpu0.o \
ch_dynamiclinker.cpu0.o libfoobar.cpu0.so lib_cpu0.o
${TOOLDIR}/llvm-objdump -elf2hex -le=false -cpu0dumpso libfoobar.cpu0.so \
> dlconfig/libso.hex
${TOOLDIR}/llvm-objdump -elf2hex -le=false -cpu0linkso a.out > cpu0.hex
cp -rf dlconfig cpu0.hex ../cpu0_verilog/.
echo "0   /* 0: big endian, 1: little endian */" > ../cpu0_verilog/cpu0.config

