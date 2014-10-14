#!/usr/bin/env bash
OS=`uname -s`
echo "OS =" ${OS}

if [ "$OS" == "Linux" ]; then
  TOOLDIR=/usr/local/llvm/test/cmake_debug_build/bin
else
  TOOLDIR=~/llvm/test/cmake_debug_build/Debug/bin
fi

if [ "$1" != "" ] && [ $1 != cpu032I ] && [ $1 != cpu032II ]; then
  echo "1st argument is cpu032I (default) or cpu032II"
  exit 1
fi
if [ "$1" == "" ]; then
  CPU=cpu032I
else
  CPU=$1
fi
echo "CPU =" "${CPU}"

bash rminput.sh

clang -target mips-unknown-linux-gnu -c start.cpp -emit-llvm -o start.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg-def.c -emit-llvm \
-o printf-stdarg-def.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg.c -emit-llvm \
-o printf-stdarg.bc
clang -DTEST_RUN -c ch14_inherit.cpp -emit-llvm -o ch14_inherit.bc
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
start.bc -o start.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
printf-stdarg-def.bc -o printf-stdarg-def.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
printf-stdarg.bc -o printf-stdarg.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
ch14_inherit.bc -o ch14_inherit.cpu0.o
${TOOLDIR}/llc -march=cpu0 -mcpu=${CPU} -relocation-model=static -filetype=obj \
lib_cpu0.ll -o lib_cpu0.o
${TOOLDIR}/lld -flavor gnu -target cpu0-unknown-linux-gnu start.cpu0.o \
printf-stdarg-def.cpu0.o printf-stdarg.cpu0.o ch14_inherit.cpu0.o lib_cpu0.o \
-o a.out
${TOOLDIR}/llvm-objdump -elf2hex a.out > ../cpu0_verilog/cpu0.hex
