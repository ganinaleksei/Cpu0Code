// clang -target mips-unknown-linux-gnu -c ch3_3.cpp -emit-llvm -o ch3_3.bc
// /usr/local/llvm/test/cmake_debug_build/bin/llc -march=cpu0 -relocation-model=pic -filetype=asm ch3_3.bc -o -

/// start
int main()
{
  volatile int a = 5;
  volatile int b = 0;

  int c = a + b;

  return c;
}

