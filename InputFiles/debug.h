#ifndef _DEBUG_H_
#define _DEBUG_H_

#define STOP \
  asm("lui $t9, 0xffff"); \
  asm("addiu $t9, $zero, 0xffff"); \
  asm("ret $t9");

#define ENABLE_TRACE \
  asm("ori $sw, $sw, 0x0020");

#define DISABLE_TRACE \
  asm("andi $sw, $sw, 0xffdf");

#endif

