// /usr/local/llvm/release/cmake_debug_build/bin/clang -target x86_64-unknown-linux-gnu -c x86_64_start.cpp -emit-llvm -o x86_64_start.bc
// /usr/local/llvm/release/cmake_debug_build/bin/llvm-dis x86_64_start.bc -o x86_64_start.ll

extern int main();

int start()
{
  return main();
}

