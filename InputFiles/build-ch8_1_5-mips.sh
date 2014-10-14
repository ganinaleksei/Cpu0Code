#!/usr/bin/env bash

OS=`uname -s`
echo "OS =" ${OS}

if [ "$OS" == "Linux" ]; then
  TOOLDIR=/usr/local/llvm/test/cmake_debug_build/bin
else
  TOOLDIR=~/llvm/test/cmake_debug_build/Debug/bin
fi

clang -target mips-unknown-linux-gnu -c printf-stdarg-def.c -emit-llvm \
-o printf-stdarg-def.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg.c -emit-llvm \
-o printf-stdarg.bc
clang -target mips-unknown-linux-gnu -c ch8_1_5.cpp -emit-llvm -o ch8_1_5.bc
clang -target mips-unknown-linux-gnu -c ch8_1_5_call.cpp -emit-llvm -o ch8_1_5_call.bc
${TOOLDIR}/llc -march=mipsel -relocation-model=static \
-filetype=obj printf-stdarg-def.bc -o printf-stdarg-def.cpu0.o
${TOOLDIR}/llc -march=mipsel -relocation-model=static \
-filetype=obj printf-stdarg.bc -o printf-stdarg.cpu0.o
${TOOLDIR}/llc -march=mipsel -relocation-model=static \
-filetype=obj ch8_1_5.bc -o ch8_1_5.cpu0.o
${TOOLDIR}/llc -march=mipsel -relocation-model=static \
-filetype=obj ch8_1_5_call.bc -o ch8_1_5_call.cpu0.o
${TOOLDIR}/llc -march=mipsel -relocation-model=static \
-filetype=obj lib_cpu0.ll -o lib_cpu0.o
${TOOLDIR}/lld -flavor gnu -target mipsel-unknown-linux-gnu \
printf-stdarg-def.cpu0.o printf-stdarg.cpu0.o ch8_1_5.cpu0.o ch8_1_5_call.cpu0.o \
lib_cpu0.o -o a.out

