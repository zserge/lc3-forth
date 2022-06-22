#ifndef LC3_H
#define LC3_H

/*
 * LC-3 Assember
 */
static int asmbuf[0xffff], *asmptr = asmbuf + 0x3000, asmrun = 0;
#define _R(n) ((n & 7) | 0xa000) /* helper to define register enum */
#define NR(r) (r & 7)            /* register number 0..7 */
#define DR(r) (NR(r) << 9)       /* destination register arg */
#define SR(r) (NR(r) << 6)       /* source register arg */
#define IR(x) (((x >> 12) == 0xa) ? NR(x) : ((x & 0x1f) | 0x20)) /* imm or reg */
#define A9(a) ((a) & 0x1ff)        /* PCOffset9 arg */

enum {R0 = _R(0), R1 = _R(1), R2 = _R(2), R3 = _R(3), R4 = _R(4), R5 = _R(5), R6 = _R(6), R7 = _R(7)};

#define _L(x)            { x; return asmptr - asmbuf - 1; }
static int PC()          { return asmptr - asmbuf; }
static int DW(int n)     { *asmptr++ = n; return PC() - 1; }
static int LL(int label) { return label - PC() - 1; }

static int BR(int addr)               { _L(*asmptr++ = 0x0e00 | A9(LL(addr))); }
static int BRN(int addr)              { _L(*asmptr++ = 0x0800 | A9(LL(addr))); }
static int BRZ(int addr)              { _L(*asmptr++ = 0x0400 | A9(LL(addr))); }
static int BRP(int addr)              { _L(*asmptr++ = 0x0200 | A9(LL(addr))); }
static int BRNZ(int addr)             { _L(*asmptr++ = 0x0c00 | A9(LL(addr))); }
static int BRNP(int addr)             { _L(*asmptr++ = 0x0a00 | A9(LL(addr))); }
static int BRZP(int addr)             { _L(*asmptr++ = 0x0600 | A9(LL(addr))); }

static int ADD(int x, int y, int z)   { _L(*asmptr++ = 0x1000 | DR(x) | SR(y) | IR(z)); }
static int AND(int x, int y, int z)   { _L(*asmptr++ = 0x5000 | DR(x) | SR(y) | IR(z)); }
static int NOT(int x, int y)          { _L(*asmptr++ = 0x9000 | DR(x) | SR(y) | 0x3f);  }

static int LEA(int x, int addr)       { _L(*asmptr++ = 0xe000 | DR(x) | A9(LL(addr))); }
static int LD(int x, int addr)        { _L(*asmptr++ = 0x2000 | DR(x) | A9(LL(addr))); }
static int LDI(int x, int addr)       { _L(*asmptr++ = 0xa000 | DR(x) | A9(LL(addr))); }
static int ST(int x, int addr)        { _L(*asmptr++ = 0x3000 | DR(x) | A9(LL(addr))); }
static int STI(int x, int addr)       { _L(*asmptr++ = 0xb000 | DR(x) | A9(LL(addr))); }
static int LDR(int x, int y, int z)   { _L(*asmptr++ = 0x6000 | DR(x) | SR(y) | (z & 0x3f)); }
static int STR(int x, int y, int z)   { _L(*asmptr++ = 0x7000 | DR(x) | SR(y) | (z & 0x3f)); }

static int JMP(int x)                 { _L(*asmptr++ = 0xc000 | SR(x)); }
static int JSR(int addr)              { _L(*asmptr++ = 0x4800 | A9(LL(addr))); }
static int RET()                      { _L(*asmptr++ = 0xc000 | SR(R7)); }

static int TRAP(int op)               { _L(*asmptr++ = 0xf000 | (op & 0xff)); }
static int GETC()                     { _L(TRAP(0x20)); }
static int PUTC()                     { _L(TRAP(0x21)); }
static int HALT()                     { _L(TRAP(0x25)); }

#define LC3ASM() for (asmrun=2; asmrun && (asmptr = asmbuf + 0x3000); asmrun--)

/*
 * LC-3 Virtual Machine
 */
static int sext(int x, int bits) {
	int m = (1 << (bits - 1));
	int n = x & ((1 << bits) - 1);
	return (n ^ m) - m;
}
static int flags(int x) { return (x < 0) * 4 + (x == 0) * 2 + (x > 0); }
static int lc3(int *mem, int *reg, int pc) {
#define OP(_hint, code, body) case code: body; return pc;
#define CC(x) reg[8] = flags(x)
  int op = mem[pc];
  int x = (op >> 9) & 7;
  int y = (op >> 6) & 7;
  int z = (op & 0x3f);
  pc++;
  switch (op >> 12) {
    OP(BR,   0, pc += (reg[8] & x) ? sext(op, 9) : 0)
    OP(ADD,  1, CC(reg[x] = reg[y] + ((z & 0x20) ? sext(op, 5) : reg[z])))
    OP(LD,   2, CC(reg[x] = mem[pc + sext(op, 9)]))
    OP(ST,   3, mem[pc + sext(op, 9)] = reg[x]);
    OP(JSR,  4, (reg[7] = pc, pc = pc + sext(op, 9)))
		OP(AND,  5, CC(reg[x] = reg[y] & ((z & 0x20) ? sext(op, 5) : reg[z])));
		OP(LDR,  6, CC(reg[x] = mem[reg[y] + sext(op, 6)]));
		OP(STR,  7, mem[reg[y] + sext(op, 6)] = reg[x]);
		OP(NOT,  9, CC(reg[x] = ~reg[y]));
		OP(LDI, 10, CC(reg[x] = mem[mem[pc + sext(op, 9)]]));
		OP(STI, 11, mem[mem[pc + sext(op, 9)]] = reg[x]);
		OP(JMP, 12, pc = reg[y]);
		OP(LEA, 14, CC(reg[x] = pc + sext(op, 9)));
		OP(TRAP,15, (z == 0x20) ? reg[0] = getchar() : (z == 0x21) ? (putchar(reg[0]),fflush(stdout)) : (pc = (z == 0x25 ? -1 : pc)));
	}
#undef OP
#undef CC
	return -1;
}

#endif /* LC3_H */
