// clang -target mips-unknown-linux-gnu -c ch8_4.cpp -emit-llvm -o ch8_4.bc
// /Users/Jonathan/llvm/test/cmake_debug_build/Debug/bin/llc -march=cpu0 -mcpu=cpu032I -relocation-model=pic -filetype=asm ch8_4.bc -o -

/// start
long long test_longlong_shift()
{
  long long a = 4;
  long long b = 0x12;
  long long c;
  long long d;
  
  c = (b >> a);  // cc = 0x1
  d = (b << a);  // cc = 0x120

  return (c+d); // 0x121 = 289
}

