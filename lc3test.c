#include <assert.h>
#include <stdio.h>

#include "lc3.h"

static void DUMP() {
	int *i;
	for (i = asmbuf + 0x3000; i < asmptr; i++) {
		printf("%lx | %04x\n", i - asmbuf, *i);
	}
}

static void RUN() {
  int pc = 0x3000;
	int reg[9] = {0};
  while (pc) {
    pc = lc3(asmbuf, reg, pc);
    /*printf("%x | %04x %04x %04x %04x %04x %04x %04x %04x\n", pc, reg[0], reg[1], reg[2], reg[3], reg[4], reg[5], reg[6], reg[7]);*/
  }
}

static void test_arithmetic() {
	int x, y, z;
	LC3ASM() {
		/* code: x - y = x + (-y) = x + (not(y) + 1) */
		LD(R1, x);
		LD(R2, y);
		NOT(R2, R2);
		ADD(R2, R2, 1);
		ADD(R3, R1, R2);
		ST(R3, z);
		HALT();
		/* data */
		x = DW(5);
		y = DW(2);
		z = DW(0);
	}
	RUN();
	assert(asmbuf[z] == 3);
	asmbuf[x] = 7;
	asmbuf[y] = 11;
	RUN();
	assert(asmbuf[z] == -4);
}

int main() {
	test_arithmetic();
	return 0;
}
