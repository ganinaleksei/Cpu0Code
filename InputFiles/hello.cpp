
// clang -c hello.c -emit-llvm -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk/usr/include/ -o hello.bc
// llc hello.bc -o hello.s
// gcc hello.s -o hello.native

/// start
#include <iostream>

using namespace std;

int main() {
  cout << "hello world" << endl;
  return 0;
}
