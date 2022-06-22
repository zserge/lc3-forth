#include <stdio.h>

#include "lc3.h"

int main() {
	int pc;
	int reg[9] = {0};

	/* load binary file, high-endian */
	for (pc = 0x3000;;pc++) {
		int hi = getchar();
		int lo = getchar();
		if (hi < 0 || lo < 0) break;
		int op = (hi << 8) + lo;
		asmbuf[pc] = op;
	}

	/* execute */
  for (pc=0x3000; (pc = lc3(asmbuf, reg, pc)) > 0;);
	return 0;
}
