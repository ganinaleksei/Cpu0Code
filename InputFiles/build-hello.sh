#!/usr/bin/env bash

if [ $# == "0" ]; then
  echo "useage: bash build-slinker.sh cpu_type endian"
  echo "  cpu_type: cpu032I or cpu032II"
  echo "  endian: be (big endian, default) or le (little endian)"
  echo "for example:"
  echo "  bash build-slinker.sh cpu032I be"
  exit 1;
fi
if [ $1 != cpu032I ] && [ $1 != cpu032II ]; then
  echo "1st argument is cpu032I or cpu032II"
  exit 1
fi

OS=`uname -s`
echo "OS =" ${OS}

if [ "$OS" == "Linux" ]; then
  TOOLDIR=/usr/local/llvm/test/cmake_debug_build/bin
else
  TOOLDIR=~/llvm/test/cmake_debug_build/Debug/bin
fi

CPU=$1
echo "CPU =" "${CPU}"

if [ "$2" != "" ] && [ $2 != le ] && [ $2 != be ]; then
  echo "2nd argument is be (big endian, default) or le (little endian)"
  exit 1
fi
if [ "$2" == "" ] || [ $2 == be ]; then
  endian=
else
  endian=el
fi
echo "endian =" "${endian}"

bash rminput.sh

clang -target mips-unknown-linux-gnu -c start.cpp -emit-llvm -o start.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg-def.c -emit-llvm \
-o printf-stdarg-def.bc
clang -target mips-unknown-linux-gnu -c printf-stdarg.c -emit-llvm \
-o printf-stdarg.bc
clang -target mips-unknown-linux-gnu -c ch_hello.c -emit-llvm -o ch_hello.bc
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj start.bc -o start.cpu0.o
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj printf-stdarg-def.bc -o printf-stdarg-def.cpu0.o
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj printf-stdarg.bc -o printf-stdarg.cpu0.o
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj ch_hello.bc -o ch_hello.cpu0.o
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj lib_cpu0.ll -o lib_cpu0.o
${TOOLDIR}/lld -flavor gnu -target cpu0${endian}-unknown-linux-gnu \
start.cpu0.o printf-stdarg-def.cpu0.o printf-stdarg.cpu0.o ch_hello.cpu0.o \
lib_cpu0.o -o a.out

endian=`${TOOLDIR}/llvm-readobj -h a.out|grep "DataEncoding"|awk '{print $2}'`
echo "endian = " "$endian"
if [ "$endian" == "LittleEndian" ] ; then
  le="true"
elif [ "$endian" == "BigEndian" ] ; then
  le="false"
else
  echo "!endian unknown"
  exit 1
fi
${TOOLDIR}/llvm-objdump -elf2hex -le=${le} a.out > ../cpu0_verilog/cpu0.hex
if [ ${le} == "true" ] ; then
  echo "1   /* 0: big endian, 1: little endian */" > ../cpu0_verilog/cpu0.config
else
  echo "0   /* 0: big endian, 1: little endian */" > ../cpu0_verilog/cpu0.config
fi

