/// start
#define CLEAR_SW \
asm("andi $sw, $ZERO, 0");

#define initRegs() \
  asm("addiu $1,  $ZERO, 0"); \
  asm("addiu $2,  $ZERO, 0"); \
  asm("addiu $3,  $ZERO, 0"); \
  asm("addiu $4,  $ZERO, 0"); \
  asm("addiu $5,  $ZERO, 0"); \
  asm("addiu $6,  $ZERO, 0"); \
  asm("addiu $7,  $ZERO, 0"); \
  asm("addiu $8,  $ZERO, 0"); \
  asm("addiu $9,  $ZERO, 0"); \
  CLEAR_SW; \
  asm("addiu $fp, $ZERO, 0");

