#!/usr/bin/env bash
OS=`uname -s`

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

clang -target mips-unknown-linux-gnu -c ch_run_backend.cpp -emit-llvm -o \
ch_run_backend.bc
${TOOLDIR}/llc -march=cpu0${endian} -mcpu=${CPU} -relocation-model=static \
-filetype=obj ch_run_backend.bc -o ch_run_backend.cpu0.o
${TOOLDIR}/llvm-objdump -d ch_run_backend.cpu0.o | tail -n +6| awk \
'{print "/* " $1 " */\t" $2 " " $3 " " $4 " " $5 "\t/* " $6"\t" $7" " $8" \
" $9" " $10 "\t*/"}' > ../cpu0_verilog/cpu0.hex

if [ $2 == le ] ; then
  echo "1   /* 0: big endian, 1: little endian */" > ../cpu0_verilog/cpu0.config
else
  echo "0   /* 0: big endian, 1: little endian */" > ../cpu0_verilog/cpu0.config
fi

