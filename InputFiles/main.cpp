// /usr/local/llvm/release/cmake_debug_build/bin/clang -c -fPIC foobar.cpp
// /home/Gamma/test/lld/cmake_debug_build/bin/lld -flavor gnu -target x86_64-unknown-linux-gnu -shared -o libfoobar.so foobar.o
// /usr/local/llvm/release/cmake_debug_build/bin/clang -target x86_64-unknown-linux-gnu -c x86_64_start.ll -o x86_64_start.o
// /usr/local/llvm/release/cmake_debug_build/bin/clang -target x86_64-unknown-linux-gnu -c main.cpp -o main.o
// /home/Gamma/test/lld/cmake_debug_build/bin/lld -flavor gnu -target x86_64-unknown-linux-gnu x86_64_start.o main.o libfoobar.so

/// start
extern int foo(int x1, int x2);
extern int bar();

int main()
{
//  ENABLE_TRACE;
  int a = 0;

  a = foo(1, 2);
  a = bar();
  
  return 0;
}
