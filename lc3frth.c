#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "lc3.h"

const int F_IMMEDIATE = 0x10; /* always interpret a word */
const int LENMASK = 0xf; /* word length limited to 15 chars */

const int TIB  = 0x4000; /* start of the terminal input buffer */
const int TOIN = 0x4100; /* next char in TIB (>IN) */
const int LATEST = 0x4101; /* last word in the dict */
const int STATE = 0x4102; /* last word in the dict */
const int HERE = 0x4103; /* last word in the dict */
const int SPADDR = 0x6000; /* data stack address */
const int RPADDR = 0x7000; /* return stack address */

const int IP = R4;
const int SP = R5;
const int RP = R6;

/*
 * LC-3 Disassembler + Debugger
 */
static char *lc3dis(int op) {
	static char *bin = "0000000100100011010001010110011110001001101010111100110111101111";
	static char *ops = "BR ADDLD ST JSRANDLDRSTRxxxNOTLDISTIJMPLEAxxxTRP";
	static char buf[128];
  int x = (op >> 9) & 7, y = (op >> 6) & 7, z = (op & 0x3f), code = op >> 12;
	snprintf(buf, sizeof(buf), "%.*s %.*s %.*s %.*s | %.*s %2x %2x %2x",
			4, &bin[((op >> 12) & 0xf) * 4], 4, &bin[((op >> 8) & 0xf) * 4],
			4, &bin[((op >> 4) & 0xf) * 4], 4, &bin[(op & 0xf) * 4],
			3, &ops[code * 3], x, y, z);
	return buf;
}

static void LC3DIS() {
	int *i;
	for (i = asmbuf + 0x3000; i < asmptr; i++) {
		printf("%lx | %04x | %s\n", i - asmbuf, *i & 0xffff, lc3dis(*i));
	}
	printf("\nSIZE: %d bytes\n\n", asmptr - asmbuf - 0x3000);
}

/* WORD makes a new word record, yay! */
static int link = 0;
static void WORD(const char *name, int flags) {
	int i;
	link = DW(link); /* link to the previous word */
	DW(strlen(name) + flags); /* length + flags */
	for (i = 0; i < strlen(name); i++) {
		DW(name[i]); /* word name */
	}
}

static void VAR(const char *name, int ptr, int next) {
	WORD(name, 0);
	LD(R0, ptr);
	ADD(SP, SP, 1);
	STR(R0, SP, 0);
	BR(next);
}

static void dump(const char *name, int addr, int size) {
	int i;
	printf("%4s | %04x | ", name, addr);
	for (i = 0; i < size; i++) {
			printf("%02x ", asmbuf[addr + i]);
	}
	printf("\n");
}

static void DEBUG(int sp, int rp) {
	dump("SP", SPADDR, sp - SPADDR + 1);
	dump("RP", RPADDR, rp - RPADDR + 1);
#if 1
	dump("TIB", TIB, 20);
	dump("", TIB+20, 20);
	dump("DICT", 0x5000, 20);
	for (int word = asmbuf[LATEST]; word; word = asmbuf[word]) {
		int i, j;
		printf("     | %04x | %02x | ", word, asmbuf[word+1]);
		for (i = 0; i < (asmbuf[word+1]&0xf); i++) {
			printf("%c", asmbuf[word + i + 2]);
		}
		printf(" => ");
		for (j = 0; j < 12; j++) {
			printf("%04x ", asmbuf[word + i + j + 5]);
			if (asmbuf[word + i + j + 5] == 0x3101) { /* 0x3101 is exit word address */
				break;
			}
		}
		printf("\n");
	}
#endif
	printf(" >IN | %04x | %x [%x]\n", TOIN, asmbuf[TOIN], asmbuf[asmbuf[TOIN]]);
	printf("LATE | %04x | %04x\n", LATEST, asmbuf[LATEST]);
	printf("HERE | %04x | %04x\n", HERE, asmbuf[HERE]);
	printf("STAT | %04x | %04x\n", STATE, asmbuf[STATE]);
	printf("\n");
	fflush(stdout);
}

static void RUN() {
  int pc = 0x3000;
	int reg[9] = {0, 0, 0, 0, 0, 0, 0, 0, 2};
	while (pc > 0) {
		if (0 || (asmbuf[pc] >= 0xf000 && asmbuf[pc] <= 0xf00f)) {
			printf("%04x | %04x | R0=%04x R1=%04x R2=%04x R3=%04x IP=%04x SP=%04x RP=%04x R7=%04x | %c%c%c\n",
					pc, asmbuf[pc],
					(uint16_t) reg[0], (uint16_t) reg[1], (uint16_t) reg[2], (uint16_t) reg[3],
					(uint16_t) reg[4], (uint16_t) reg[5], (uint16_t) reg[6], (uint16_t) reg[7],
					reg[8] & 4 ? 'n' : '-', reg[8] & 2 ? 'z' : '-', reg[8] & 1 ? 'p' : '-');
			DEBUG(reg[5], reg[6]);
		}
		pc = lc3(asmbuf, reg, pc);
	}
}

static int words(int next) {
	/* @ ( addr -- x )       Fetch memory at addr */
	WORD("@", 0);
	LDR(R0, SP, 0);
	LDR(R0, R0, 0);
	STR(R0, SP, 0);
	BR(next);

	/* ! ( x addr -- )       Store x at addr */
	WORD("!", 0);
	LDR(R0, SP, 0);
	LDR(R1, SP, -1);
	STR(R1, R0, 0);
	ADD(SP, SP, -2);
	BR(next);
	
	/* sp@ ( -- addr )       Get current data stack pointer */
	WORD("sp@", 0);
	STR(SP, SP, 1);
	ADD(SP, SP, 1);
	BR(next);

	/* rp@ ( -- addr )       Get current return stack pointer */
	WORD("rp@", 0);
	ADD(SP, SP, 1);
	STR(RP, SP, 0);
	BR(next);

	/* 0= ( x -- f )         -1 if top of stack is 0, 0 otherwise */
	WORD("0=", 0);
	LDR(R0, SP, 0);
	BRNP(PC()+3);
	NOT(R0, R0);
	BR(PC()+2);
	AND(R0, R0, 0);
	STR(R0, SP, 0);
	BR(next);

	/* + ( x1 x2 -- n )      Add the two values at the top of the stack */
	WORD("+", 0);
	LDR(R0, SP, 0);
	LDR(R1, SP, -1);
	ADD(R1, R1, R0);
	ADD(SP, SP, -1);
	STR(R1, SP, 0);
	BR(next);

	/* nand ( x1 x2 -- n )   NAND the two values at the top of the stack */
	WORD("nand", 0);
	LDR(R0, SP, 0);
	LDR(R1, SP, -1);
	AND(R1, R1, R0);
	NOT(R1, R1);
	ADD(SP, SP, -1);
	STR(R1, SP, 0);
	BR(next);

	/* key ( -- k )          Read key from console */
	WORD("key", 0);
	TRAP(0x20);
	ADD(SP, SP, 1);
	STR(R0, SP, 0);
	BR(next);

	/* emit ( c -- )         Write character to console */
	WORD("emit", 0);
	LDR(R0, SP, 0);
	ADD(SP, SP, -1);
	TRAP(0x21);
	BR(next);

#if 0
	WORD("debug", 0);
	TRAP(0x0);
	BR(next);
#endif

	WORD("1", 0);
	AND(R0, R0, 0);
	ADD(R0, R0, 1);
	ADD(SP, SP, 1);
	STR(R0, SP, 0);
	BR(next);

	/* bye ( -- ) Terminate */
	WORD("bye", 0);
	TRAP(0x25);
	BR(next);

	return link;
}

static int parser() {
	static int tibaddr, toinaddr, latest, space, question, state, here;
	static int next, loop, rpaddr, spaddr;
	static int readline, readloop, token, skipws, findws, done, 
						 intrp, start, ismtch, nomtch, found, error, nextc;
	static int copy;
	static int docol;
	static int compile;
	static int exit;


	/* error: print "??" and reset */
	error =  LD(R0, question);   /* r0 := '?' */
	         TRAP(0x21);         /* putchar, fallthrough to start*/

  /* main entry point: set >IN to the start of TIB, write null-terminator to TIB[0] */
	start =  LD(R0, toinaddr);   /* r0 := &>IN */
	         LD(R1, tibaddr);    /* r1 := &TIB */
					 STR(R1, R0, 0);     /* make >IN point at the start of TIB */
					 AND(R2, R2, 0);     /* r2 := 0 */
					 STR(R2, R1, 0);     /* TIB[0] = 0 */
					 LD(SP, spaddr);     /* SP := start of data stack */
					 LD(RP, rpaddr);     /* RP := start of return stack */

	/* interpreter: read/eval loop */
	intrp  = JSR(token);         /* r1, r2 := (start, length) of the token in TIB */
	         STR(R1, SP, 2);
	         STR(R2, SP, 3);
	         LDI(R0, latest);    /* r0 := [LATEST] */
	ismtch = BRZ(error);         /* if r0 == 0: goto error (word not found) */
					 STR(R0, SP, 1);     /* push word code onto SP stack */
	         LDR(R1, SP, 2);
	         LDR(R2, SP, 3);
					 LDR(R3, R0, 1);     /* r3 := [link+1] (word length) */
					 AND(R3, R3, 0xf);   /* r3 := word length */
					 NOT(R3, R3);
					 ADD(R3, R3, 1);
					 ADD(R3, R3, R2);    /* r3 := r3 - r2 (compare length) */
					 BRNP(nomtch);       /* if length differs - try next word */
	nextc  = LDR(IP, R0, 2);     /* ip := [r0+2] (nth word letter from dict) */
					 LDR(R3, R1, 0);     /* r2 := [r1]   (nth word letter from token) */
					 NOT(R3, R3);
					 ADD(R3, R3, 1);
					 ADD(R3, R3, IP);    /* compare ip and r2 */
					 BRNP(nomtch);       /* if letters don't match - try next word */
					 ADD(R1, R1, 1);     /* advance letter counter for token */
					 ADD(R0, R0, 1);     /* advance letter counter for dict */
					 ADD(R2, R2, -1);    /* decrease remaining length */
					 BRZ(found);         /* if remaining length == 0: found a word! */
					 BR(nextc);
	nomtch = LDR(R0, SP, 1);     /* restore link */
		       LDR(R0, R0, 0);     /* r0 := [r0] (next link) */
	         BR(ismtch);
  found  = ADD(R0, R0, 2);
					 LEA(IP, loop);
					 LDI(R1, state);
					 LDR(R2, SP, 1);     /* restore word code from SP */
					 LDR(R2, R2, 1);     /* get word length and flags */
					 AND(R2, R2, 0x10);
					 ADD(R2, R2, R1);
					 ADD(R2, R2, -1);
					 BRZ(compile);
					 JMP(R0);

	loop   = DW(intrp);

	/* token: parse a word from TIB, return its address (R1) and length (R2) */
	token  = LD(R2, toinaddr);   /* r2 := hint for >IN address */
	         LDR(R2, R2, 0);
					 LD(R3, space);      /* use r3 as ' ' (0x20) because ADD can't use imm 0x20 */
	skipws = LDR(R0, R2, 0);     /* r0 := mem[r2] */
	         BRZ(readline);      /* if r0 is terminating zero - read another TIB */
	         AND(R1, R2, R2);    /* r1 := r2 */
	         ADD(R2, R2, 1);     /* r2 := r2 + 1, next char */
	         ADD(R0, R0, R3);    /* is r0 == ' ' (in two steps) */
	         BRZ(skipws);
	         ADD(R2, R2, -1);    /* r2 := r2 - 1, roll back a bit for the next loop */
	findws = ADD(R2, R2, 1);     /* r2 := r2 + 1, next char */
				 	 LDR(R0, R2, 0);     /* r0 := mem[r2] */
	         BRZ(done);          /* if r0 is terminating zero - done */
					 ADD(R0, R0, R3);    /* is r0 == ' ', we can't use 32 so we subtract twice */
	         BRNP(findws);
	done =   LD(R0, toinaddr);   /* update >IN */
	         STR(R2, R0, 0);     /* points right after the end of the token */
					 NOT(R3, R1);        /* r3 := ~r1 + 1 (or r3 := -r1) */
					 ADD(R3, R3, 1);    
					 ADD(R2, R2, R3);    /* r2 := r2 - r1 (length of the word) */
	         RET();

	/* readline: read a line until CR and fill in the TIB */
	readline = LD(R1, tibaddr);    /* load TIB address */
	           LD(R0, toinaddr);   /* load >IN address */
	           STR(R1, R0, 0);     /* make >IN point at the start of TIB */
	readloop = TRAP(0x20);         /* r0 = getchar() */
	           STR(R0, R1, 0);     /* mem[r1] = r0 */
	           ADD(R1, R1, 1);     /* r1 = r1 + 1 */
	           ADD(R0, R0, -10);   /* is r0 == '\n' ? */
	           BRNP(readloop);     /* no: read another character */
	           ADD(R1, R1, -1);    /* r1 = r1 - 1 to erase newline */
	           STR(R0, R1, 0);     /* write 0 instead */
						 BR(token);          /* try to parse first token */

	space = DW(-32);
	question = DW('?');
	toinaddr = DW(TOIN);
	tibaddr = DW(TIB);
	latest = DW(LATEST);
	state = DW(STATE);
	here = DW(HERE);
	spaddr = DW(SPADDR);
	rpaddr = DW(RPADDR);

	next   = LDR(R0, IP, 0);
			     ADD(IP, IP, 1);
					 JMP(R0);

	link = 0;

	words(next);

	VAR("tib", tibaddr, next);
	VAR(">in", toinaddr, next);
	VAR("latest", latest, next);
	VAR("state", state, next);
	VAR("here", here, next);

  static int op1, op2, op3;

	         /* exit ( r:addr -- )    Resume execution at address at the top of the return stack */
	         WORD("exit", 0);
	  exit = LDR(IP, RP, 0);
				   ADD(RP, RP, -1);
			     BR(next);

						WORD(";", F_IMMEDIATE);
						AND(R1, R1, 0);
						STI(R1, state);
						LEA(R0, exit);
  compile = LDI(R1, here);
						STR(R0, R1, 0);
						ADD(R1, R1, 1);
						STI(R1, here);
						BR(next);

	         WORD(":", 0);
           /* read next token after colon */
           JSR(token);
           /* update latest to point to the current word record */
           LDI(R3, here);   /* r3 := here */
           LDI(R0, latest); /* r0 := latest */
           /* write link (previous "latest") at [HERE] */
           STR(R0, R3, 0); /* [here] := latest */
           STI(R3, latest); /* [latest] := here */
           /* write word length at [HERE+1] */
           STR(R2, R3, 1);
           ADD(R3, R3, 2);
           /* copy from R1..(R1+R2) to HERE */
    copy = LDR(R0, R1, 0);
           STR(R0, R3, 0);
           ADD(R1, R1, 1);
           ADD(R3, R3, 1);
           ADD(R2, R2, -1);
           BRP(copy);
           /* compile absolute jump to docol */
					 LD(R0, op1);
					 STR(R0, R3, 0);
					 LD(R0, op2);
					 STR(R0, R3, 1);
					 LD(R0, op3);
					 STR(R0, R3, 2);
           ADD(R3, R3, 3);
           /* update HERE := R3 */
           STI(R3, here);
           /* set state to "compiling" */
           AND(R1, R1, 0);
           ADD(R1, R1, 1); /* r1 := 1 */
           STI(R1, state); /* [state] = r1 */
           BR(next);

	op1 =    DW(0x2201);  /* LD(R1, PC+1) */
	op2 =    DW(0xc040);  /* JMP(R1)      */
	op3 =    DW(docol);   /* FILL [docol] */

	docol =  STR(IP, RP, 1);  /* store current IP on return stack */
					 ADD(RP, RP, 1);
					 ADD(IP, R0, 3);  /* skip compiled LD+JMP+FILL and advance IP */
					 BR(next);

	asmbuf[LATEST] = link;
	asmbuf[HERE] = 0x5000; //asmptr - asmbuf;

	return start;
}

int main() {
	/* A two-pass assembler is probably needed */
	int start;
	LC3ASM() {
		asmptr = asmbuf + 0x3000;
		BR(start);
		start = parser();
	}
	LC3DIS();
	RUN();
	return 0;
}
