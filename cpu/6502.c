#include "6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "globals.h"
#include "ppu.h"
#include "apu.h"
#include "mapper.h"
#include "cartridge.h"

reset_t rstFlag;

						 /* 0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
static uint_fast8_t ctable[] = { 7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6, /* 0 */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, /* 1 */
							6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6, /* 2 */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, /* 3 */
							6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6, /* 4 */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, /* 5 */
							6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6, /* 6 */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, /* 7 */
							2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4, /* 8 */
							2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5, /* 9 */
							2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4, /* a */
							2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, /* b */
							2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, /* c */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, /* d */
							2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, /* e */
							2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7  /* f */
							};

static inline void write_cpu_register(uint16_t, uint_fast8_t), power_reset(void), cpuwrite(uint16_t, uint_fast8_t), interrupt_polling();
static inline uint_fast8_t read_cpu_register(uint16_t);
static inline void accum(), immed(), zpage(), zpagex(), zpagey(), absol(), absxR(), absxW(), absyR(), absyW(), indx(), indyR(), indyW();
static inline void adc(), ahx(), alr(), and(), anc(), arr(), asl(), asli(), axs(), branch(), bit(), brkop(), clc(), cld(),
				   cli(), clv(), cmp(), cpx(), cpy(), dcp(), dec(), dex(), dey(), eor(), inc(), isc(), inx(), iny(), jmpa(), jmpi(), jsr(), las(),
				   lax(), lda(), ldx(), ldy(), lsr(), lsri(), nopop(), ora(), pha(), php(), pla(), plp(), rla(), rra(), rol(), roli(), ror(), rori(), rti(), rts(),
				   sax(), sbc(), sec(), sed(), sei(), shx(), shy(), slo(), sre(), sta(), stx(), sty(), tas(), tax(), tay(), tsx(), txa(), txs(),
				   tya(), xaa(), none();

uint_fast8_t mode, opcode, addmode, addcycle, tmpval8, s = 0, dummy, pcl, pch, dummywrite = 0, op, irqPulled = 0, nmiPulled = 0, irqPending = 0, nmiPending = 0, intDelay = 0;
uint16_t addr, tmpval16;

/* Mapped memory */
uint_fast8_t *prgSlot[0x8], cpuRam[0x800];

/* Internal registers */
uint_fast8_t cpuA = 0x00, cpuX = 0x00, cpuY = 0x00, cpuP = 0x00, cpuS = 0x00;
uint16_t cpuPC;
uint32_t cpucc = 0;

/* Vector pointers */
static const uint16_t nmi = 0xfffa, rst = 0xfffc, irq = 0xfffe;

void opdecode() {

									 /* 0    | 1      2   | 3    | 4     | 5     | 6     | 7    |  8   | 9    | a    | b     | c     | d     | e     | f       */
	static void (*addtable[0x100])() = { none,  indx, none,  indx,  zpage,  zpage,  zpage,  zpage, none, immed, accum, immed, absol, absol, absol, absol, /* 0 */
									     none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW, /* 1 */
										 none,  indx, none,  indx,  zpage,  zpage,  zpage,  zpage, none, immed, accum, immed, absol, absol, absol, absol, /* 2 */
										 none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW, /* 3 */
										 none,  indx, none,  indx,  zpage,  zpage,  zpage,  zpage, none, immed, accum, immed,  none, absol, absol, absol, /* 4 */
										 none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW, /* 5 */
										 none,  indx, none,  indx,  zpage,  zpage,  zpage,  zpage, none, immed, accum, immed,  none, absol, absol, absol, /* 6 */
										 none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW, /* 7 */
										immed,  indx, immed, indx,  zpage,  zpage,  zpage,  zpage, none, immed,  none, immed, absol, absol, absol, absol, /* 8 */
										 none, indyW, none, indyW, zpagex, zpagex, zpagey, zpagey, none,  absyR, none, absyW, absxW, absxW, absyW, absyW, /* 9 */
										immed,  indx, immed, indx,  zpage,  zpage,  zpage,  zpage, none, immed,  none, immed, absol, absol, absol, absol, /* a */
										 none, indyR, none, indyR, zpagex, zpagex, zpagey, zpagey, none,  absyR, none, absyW, absxR, absxR, absyR, absyR, /* b */
										immed,  indx, immed, indx,  zpage,  zpage,  zpage,  zpage, none, immed,  none, immed, absol, absol, absol, absol, /* c */
										 none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW, /* d */
										immed,  indx, immed, indx,  zpage,  zpage,  zpage,  zpage, none, immed,  none, immed, absol, absol, absol, absol, /* e */
										 none, indyR, none, indyW, zpagex, zpagex, zpagex, zpagex, none,  absyR, none, absyW, absxR, absxR, absxW, absxW  /* f */
									   };


				     				 /* 0    | 1  | 2    | 3  | 4    | 5  | 6  | 7  | 8  | 9  | a    | b  | c    | d  | e  | f         */
	static void (*optable[0x100])() = { brkop, ora,  none, slo, nopop, ora, asl, slo, php, ora,  asli, anc, nopop, ora, asl, slo, /* 0 */
									   branch, ora,  none, slo, nopop, ora, asl, slo, clc, ora, nopop, slo, nopop, ora, asl, slo, /* 1 */
									      jsr, and,  none, rla,   bit, and, rol, rla, plp, and,  roli, anc,   bit, and, rol, rla, /* 2 */
									   branch, and,  none, rla, nopop, and, rol, rla, sec, and, nopop, rla, nopop, and, rol, rla, /* 3 */
									      rti, eor,  none, sre, nopop, eor, lsr, sre, pha, eor,  lsri, alr,  jmpa, eor, lsr, sre, /* 4 */
									   branch, eor,  none, sre, nopop, eor, lsr, sre, cli, eor, nopop, sre, nopop, eor, lsr, sre, /* 5 */
									      rts, adc,  none, rra, nopop, adc, ror, rra, pla, adc,  rori, arr,  jmpi, adc, ror, rra, /* 6 */
									   branch, adc,  none, rra, nopop, adc, ror, rra, sei, adc, nopop, rra, nopop, adc, ror, rra, /* 7 */
								        nopop, sta, nopop, sax,   sty, sta, stx, sax, dey, nopop, txa, xaa,   sty, sta, stx, sax, /* 8 */
								       branch, sta,  none, ahx,   sty, sta, stx, sax, tya, sta,   txs, tas,   shy, sta, shx, ahx, /* 9 */
								          ldy, lda,   ldx, lax,   ldy, lda, ldx, lax, tay, lda,   tax, lax,   ldy, lda, ldx, lax, /* a */
								       branch, lda,  none, lax,   ldy, lda, ldx, lax, clv, lda,   tsx, las,   ldy, lda, ldx, lax, /* b */
								          cpy, cmp, nopop, dcp,   cpy, cmp, dec, dcp, iny, cmp,   dex, axs,   cpy, cmp, dec, dcp, /* c */
								       branch, cmp,  none, dcp, nopop, cmp, dec, dcp, cld, cmp, nopop, dcp, nopop, cmp, dec, dcp, /* d */
								          cpx, sbc, nopop, isc,   cpx, sbc, inc, isc, inx, sbc, nopop, sbc,   cpx, sbc, inc, isc, /* e */
								       branch, sbc,  none, isc, nopop, sbc, inc, isc, sed, sbc, nopop, isc, nopop, sbc, inc, isc  /* f */
								  };
	/* static void (*cpuSlot[0x8])(uint_fast16_t, uint_fast8_t) { write_cpu_ram, write_ppu_register, write_cpu_register,  }; */


	if (rstFlag)
		power_reset();
	if (nmiPending)
	{
		apu_wait += 7;
		vdp_wait += 7 * 3;
		cpucc += 7;
		interrupt_handle(NMI);
		nmiPending = 0;
		intDelay = 0;
	} else if (irqPending && !intDelay) {
		apu_wait += 7;
		vdp_wait += 7 * 3;
		cpucc += 7;
		interrupt_handle(IRQ);
		irqPending = 0;
	}
	else
	{
		intDelay = 0;
		op = cpuread(cpuPC++);
/*	fprintf(logfile,"%04X %02X\t\t A:%02X X=%02X Y:%02X P:%02X SP:%02X CYC:%i\n",cpuPC-1,op,cpuA,cpuX,cpuY,cpuP,cpuS,ppudot); */
		addcycle = 0;
		apu_wait += ctable[op];
		vdp_wait += ctable[op] * 3;
		cpucc += ctable[op];
		(*addtable[op])();
		(*optable[op])();
	}
}

/* unimplemented opcodes */

void ahx() {

}

void alr() {

}

void anc() {

}

void arr() {

}

void axs() {

}

void dcp() {

}

void isc() {

}

void las() {
	synchronize(0);
}

void lax() {
	synchronize(0);
}

void rla() {

}

void rra() {

}

void sax() {

}

void shx() {

}

void shy() {

}

void slo() {

}

void sre() {

}

void tas() {

}

void xaa() {

}

/* --------- */


/*ADDRESS MODES */
void accum() {
	dummy = cpuread(cpuPC);	/* cycle 2 */
}

void immed() {
	addr = cpuPC++;	/* cycle 2 */
}

void zpage() {
	addr = cpuread(cpuPC++);			/* cycle 2 */
}

void zpagex() {
	addr = cpuread(cpuPC++);			/* cycle 2 */
	dummy = cpuread(addr);
	addr = ((addr + cpuX) & 0xff);		/* cycle 3 */
}

void zpagey() {
	addr = cpuread(cpuPC++);			/* cycle 2 */
	dummy = cpuread(addr);
	addr = ((addr + cpuY) & 0xff);		/* cycle 3 */
}

void absol() {
	addr = cpuread(cpuPC++);			/* cycle 2 */
	addr += cpuread(cpuPC++) << 8;		/* cycle 3 */
}

void absxR() {
	pcl = cpuread(cpuPC++);					/* cycle 2 */
	pch = cpuread(cpuPC++);
	pcl += cpuX;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	if ((addr & 0xff) < cpuX) {				/* cycle 5 (optional) */
		dummy = cpuread(addr);
		addr += 0x100;
		addcycle = 1;
	}
}

void absxW() {
	pcl = cpuread(cpuPC++);					/* cycle 2 */
	pch = cpuread(cpuPC++);
	pcl += cpuX;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	dummy = cpuread(addr);
	if ((addr & 0xff) < cpuX) {				/* cycle 5 (optional) */
		addr += 0x100;
		addcycle = 1;
	}
}

void absyR() {
	pcl = cpuread(cpuPC++);					/* cycle 2 */
	pch = cpuread(cpuPC++);
	pcl += cpuY;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	if ((addr & 0xff) < cpuY) {				/* cycle 5 (optional) */
		dummy = cpuread(addr);
		addr += 0x100;
		addcycle = 1;
	}
}

void absyW() {
	pcl = cpuread(cpuPC++);					/* cycle 2 */
	pch = cpuread(cpuPC++);
	pcl += cpuY;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	dummy = cpuread(addr);
	if ((addr & 0xff) < cpuY) {				/* cycle 5 (optional) */
		addr += 0x100;
		addcycle = 1;
	}
}

void indx() {
	tmpval8 = cpuread(cpuPC++);						/* cycle 2 */
	dummy = cpuread(tmpval8);						/* cycle 3 */
	pcl = cpuread(((tmpval8+cpuX) & 0xff));			/* cycle 4 */
	pch = cpuread(((tmpval8+cpuX+1) & 0xff));			/* cycle 5 */
	addr = ((pch << 8) | pcl);						/* cycle 6 */
}

void indyR() {
	tmpval8 = cpuread(cpuPC++);						/* cycle 2 */
	pcl = cpuread(tmpval8++);						/* cycle 3 */
	pch = cpuread((tmpval8 & 0xff));
	pcl += cpuY;										/* cycle 4 */
	addr = ((pch << 8) | pcl);						/* cycle 5 */
	if ((addr & 0xff) < cpuY) {						/* cycle 6 (optional) */
		dummy = cpuread(addr);
		addr += 0x100;
		addcycle = 1;
	}
}

void indyW() {
	tmpval8 = cpuread(cpuPC++);						/* cycle 2 */
	pcl = cpuread(tmpval8++);						/* cycle 3 */
	pch = cpuread((tmpval8 & 0xff));
	pcl += cpuY;										/* cycle 4 */
	addr = ((pch << 8) | pcl);						/* cycle 5 */
	dummy = cpuread(addr);
	if ((addr & 0xff) < cpuY) {						/* cycle 6 (optional) */
		addr += 0x100;
		addcycle = 1;
	}
}

		/* OPCODES */
void adc() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	tmpval16 = cpuA + tmpval8 + (cpuP & 1);
	bitset(&cpuP, (cpuA ^ tmpval16) & (tmpval8 ^ tmpval16) & 0x80, 6);
	bitset(&cpuP, tmpval16 > 0xff, 0);
	cpuA = tmpval16;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void and() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	cpuA &= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void asl() {
	synchronize(2);
	tmpval8 = cpuread(addr);			/* cycle 4 */
	cpuwrite(addr,tmpval8);				/* cycle 5 */
	bitset(&cpuP, tmpval8 & 0x80, 0);
	tmpval8 = tmpval8 << 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);								/* cycle 6 */
	dummywrite = 0;
}

void asli() {
	synchronize(1);
	interrupt_polling();
	bitset(&cpuP, cpuA & 0x80, 0);
	tmpval8 = cpuA;			/* cycle 4 */
	cpuA = tmpval8;
	tmpval8 = tmpval8 << 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	cpuA = tmpval8;
}

void bit() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, !(cpuA & tmpval8), 1);
	bitset(&cpuP, tmpval8 & 0x80, 7);
	bitset(&cpuP, tmpval8 & 0x40, 6);
}

int pageCross;
void branch() {
	synchronize(1);
	interrupt_polling();
	uint_fast8_t reflag[4] = { 7, 6, 0, 1 };
	/* fetch operand */											/* cycle 2 */
	if (((cpuP >> reflag[(op >> 6) & 3]) & 1) == ((op >> 5) & 1)) {
		if (((cpuPC + 1) & 0xff00)	!= ((cpuPC + ((int8_t) cpuread(cpuPC) + 1)) & 0xff00)) {
			apu_wait += 1;
			vdp_wait += 3;
			cpucc += 1;
			/* correct? */
			synchronize(1);
			interrupt_polling();
			pageCross = 1;
		}
		else
			pageCross = 0;
		/* prefetch next opcode, optionally add operand to pc*/	/* cycle 3 (branch) */
		cpuPC = cpuPC + (int8_t) cpuread(cpuPC) + 1;

		/* fetch next opcode if branch taken, fix PCH */		/* cycle 4 (optional) */
		/* fetch opcode if page boundary */						/* cycle 5 (optional) */
		apu_wait += 1;
		vdp_wait += 3;
		cpucc += 1;
		if (pageCross) /* special case, non-page crossing + branch taking ignores int. */
		{
		synchronize(1);
		interrupt_polling();
		}
	} else
		cpuPC++;													/* cycle 3 (no branch) */
}

void brkop() {
	cpuPC++;
	interrupt_handle(BRK);
}

void clc() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 0, 0);
}

void cld() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 0, 3);
}

void cli() {
	synchronize(1); /* delay interrupt if happen here */
	intDelay = 1;
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 0, 2);
}

void clv() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 0, 6);
}

void cmp() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, (cpuA - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuA == tmpval8, 1);
	bitset(&cpuP, cpuA >= tmpval8, 0);
}

void cpx() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);
	bitset(&cpuP, (cpuX - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuX == tmpval8, 1);
	bitset(&cpuP, cpuX >= tmpval8, 0);
}

void cpy() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);
	bitset(&cpuP, (cpuY - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuY == tmpval8, 1);
	bitset(&cpuP, cpuY >= tmpval8, 0);
}

/* DCP (r-m-w) */

void dec() {
	synchronize(2);
	tmpval8 = cpuread(addr);					/* cycle 4 */
	cpuwrite(addr,tmpval8);						/* cycle 5 */
	tmpval8 = tmpval8-1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);									/* cycle 6 */
	dummywrite = 0;
}

void dex() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuX--;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void dey() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuY--;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void eor() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);								/* cycle 4 */
	cpuA ^= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void inc() {
	synchronize(2);
	tmpval8 = cpuread(addr);				/* cycle 4 */
	cpuwrite(addr,tmpval8);					/* cycle 5 */
	tmpval8 = tmpval8 + 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);					/* cycle 6 */
	dummywrite = 0;
}

void inx() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuX++;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void iny() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuY++;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

/* ISB (r-m-w) */

void jmpa() {
	addr = cpuread(cpuPC++);			/* cycle 2 */
	synchronize(1);
	interrupt_polling();
	addr += cpuread(cpuPC++) << 8;		/* cycle 3 */
	cpuPC = addr;
}

void jmpi() {
	tmpval8 = cpuread(cpuPC++);								/* cycle 2 */
	tmpval16 = (cpuread(cpuPC) << 8);							/* cycle 3 */
	addr = cpuread(tmpval16 | tmpval8);					/* cycle 4 */
	synchronize(1);
	interrupt_polling();
	addr += cpuread(tmpval16 | ((tmpval8+1) & 0xff)) << 8;	/* cycle 5 */
	cpuPC = addr;
}

void jsr() {
	cpuwrite((0x100 + cpuS--), ((cpuPC + 1) & 0xff00) >> 8);	/* cycle 4 */
	cpuwrite((0x100 + cpuS--), ((cpuPC + 1) & 0x00ff));		/* cycle 5 */
	addr = cpuread(cpuPC++);								/* cycle 2 */
	/* internal operation? */							/* cycle 3 */
	synchronize(1);
	interrupt_polling();
	addr += cpuread(cpuPC) << 8;							/* cycle 6 */
	cpuPC = addr;
}

/* LAX (read instruction) */

void lda() {
	synchronize(1);
	interrupt_polling();
	cpuA = cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void ldx() {
	synchronize(1);
	interrupt_polling();
	cpuX = cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void ldy() {
	synchronize(1);
	interrupt_polling();
	cpuY = cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void lsr() {
	synchronize(2);
	tmpval8 = cpuread(addr);			/* cycle 4 */
	cpuwrite(addr,tmpval8);				/* cycle 5 */
	bitset(&cpuP, tmpval8 & 1, 0);
	tmpval8 = tmpval8 >> 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);				/* cycle 6 */
	dummywrite = 0;
}

void lsri() {
	synchronize(1);
	interrupt_polling();
	bitset(&cpuP, cpuA & 1, 0);
	tmpval8 = cpuA;						/* cycle 4 */
	cpuA = tmpval8;						/* cycle 5 */
	tmpval8 = tmpval8 >> 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	cpuA = tmpval8;						/* cycle 6 */
}

void nopop() {
	synchronize(1);
	interrupt_polling();
}

void ora() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	cpuA |= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void pha() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);			/* cycle 2 */
	cpuwrite((0x100 + cpuS--), cpuA);		/* cycle 3 */
}

void php() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);			/* cycle 2 */
	cpuwrite((0x100 + cpuS--), (cpuP | 0x30)); /* bit 4 is set if from an instruction */
}									/* cycle 3 */

void pla() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);			/* cycle 2 */
	/* inc sp */					/* cycle 3 */
	cpuA = cpuread(++cpuS + 0x100);		/* cycle 4 */
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void plp() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);			/* cycle 2 */
	/* inc sp */					/* cycle 3 */
	cpuP = cpuread(++cpuS + 0x100);	/* cycle 4 */
	bitset(&cpuP, 1, 5);
	bitset(&cpuP, 0, 4); /* b flag should be discarded */
}

/* RLA (r-m-w) */

void rol() {
	uint_fast8_t bkp;
	synchronize(2);
	tmpval8 = cpuread(addr);			/* cycle 4 */
	cpuwrite(addr,tmpval8);				/* cycle 5 */
	bkp = tmpval8;
	tmpval8 = tmpval8 << 1;
	bitset(&tmpval8, cpuP & 1, 0);
	bitset(&cpuP, bkp & 0x80, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);				/* cycle 6 */
	dummywrite = 0;
}

void roli() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuA;			/* cycle 4 */
	cpuA = tmpval8;						/* cycle 5 */
	tmpval8 = tmpval8 << 1;
	bitset(&tmpval8, cpuP & 1, 0);
	bitset(&cpuP, cpuA & 0x80, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	cpuA = tmpval8;								/* cycle 6 */
}

void ror() {
	uint_fast8_t bkp;
	synchronize(2);
	tmpval8 = cpuread(addr);					/* cycle 4 */
	cpuwrite(addr,tmpval8);						/* cycle 5 */
	bkp = tmpval8;
	tmpval8 = tmpval8 >> 1;
	bitset(&tmpval8, cpuP & 1, 7);
	bitset(&cpuP, bkp & 1, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	cpuwrite(addr,tmpval8);						/* cycle 6 */
	dummywrite = 0;
}

void rori() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuA;					/* cycle 4 */
	cpuA = tmpval8;						/* cycle 5 */
	tmpval8 = tmpval8 >> 1;
	bitset(&tmpval8, cpuP & 1, 7);
	bitset(&cpuP, cpuA & 1, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	cpuA = tmpval8;								/* cycle 6 */
}

/* RRA (r-m-w) */

void rti() {
	dummy = cpuread(cpuPC);					/* cycle 2 */
	/* stack inc */							/* cycle 3 */
	cpuP = cpuread(++cpuS + 0x100);			/* cycle 4 */
	bitset(&cpuP, 1, 5); /* bit 5 always set */
	bitset(&cpuP, 0, 4); /* b flag should be discarded */
	cpuPC = cpuread(++cpuS + 0x100);			/* cycle 5 */
	synchronize(1);
	interrupt_polling();
	cpuPC += (cpuread(++cpuS + 0x100) << 8);	/* cycle 6 */
}

void rts() {
	dummy = cpuread(cpuPC);					/* cycle 2 */
	/* stack inc */							/* cycle 3 */
	addr = cpuread(++cpuS + 0x100);			/* cycle 4 */
	addr += cpuread(++cpuS + 0x100) << 8;	/* cycle 5 */
	synchronize(1);
	interrupt_polling();
	cpuPC = addr + 1;							/* cycle 6 */
}

/* SAX (write instruction) */

void sbc() {
	synchronize(1);
	interrupt_polling();
	tmpval8 = cpuread(addr);						/* cycle 4 */
	tmpval16 = cpuA + (tmpval8 ^ 0xff) + (cpuP & 1);
	bitset(&cpuP, (cpuA ^ tmpval16) & (tmpval8 ^ cpuA) & 0x80, 6);
	bitset(&cpuP, tmpval16 > 0xff, 0);
	cpuA = tmpval16;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void sec() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 1, 0);
}

void sed() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 1, 3);
}

void sei() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	bitset(&cpuP, 1, 2);
}

/* SLO (r-m-w) */

/* SRE (r-m-w) */

void sta() {
	tmpval8 = cpuA;
	synchronize(1);
	interrupt_polling();
	cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void stx() {
	tmpval8 = cpuX;
	synchronize(1);
	interrupt_polling();
	cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void sty() {
	tmpval8 = cpuY;
	synchronize(1);
	interrupt_polling();
	cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void tax() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuX = cpuA;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void tay() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuY = cpuA;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void tsx() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuX = cpuS;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void txa() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuA = cpuX;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void txs() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuS = cpuX;
}

void tya() {
	synchronize(1);
	interrupt_polling();
	dummy = cpuread(cpuPC);
	cpuA = cpuY;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void none() {
							}

uint_fast8_t read_cpu_register(uint16_t address) {
	uint_fast8_t value;
	switch (address) {
	case 0x4015: /* APU status read */
		value = (dmcInt ? 0x80 : 0) | (frameInt ? 0x40 : 0) | ((dmcBytesLeft) ? 0x10 : 0) | (noiseLength ? 0x08 : 0) | (triLength ? 0x04 : 0) | (pulse2Length ? 0x02 : 0) | (pulse1Length ? 0x01 : 0);
		/* TODO: timing related inhibition of frameInt clear */
		frameInt = 0;
		break;
	case 0x4016: /* TODO: proper open bus emulation */
		value = ((ctr1 >> ctrb) & 1) | 0x40;
		if (s == 0)
			ctrb++;
		break;
	case 0x4017:
		value = ((ctr2 >> ctrb2) & 1) | 0x40;
		if (s == 0)
			ctrb2++;
		break;
	}
	return value;
}

void write_cpu_register(uint16_t address, uint_fast8_t value) {
	uint16_t source;
	switch (address) {
	case 0x4000: /* Pulse 1 duty, envel., volume */
		pulse1Control = value;
		env1Divide = (pulse1Control&0xf);
		break;
	case 0x4001: /* Pulse 1 sweep, period, negate, shift */
		sweep1 = value;
		sweep1Divide = ((sweep1>>4)&7);
		sweep1Shift = (sweep1&7);
		sweep1Reload = 1;
		break;
	case 0x4002: /* Pulse 1 timer low */
		pulse1Timer = (pulse1Timer&0x700) | value;
		break;
	case 0x4003: /* Pulse 1 length counter, timer high */
		if (apuStatus & 1)
			pulse1Length = lengthTable[((value>>3)&0x1f)];
		pulse1Timer = (pulse1Timer&0xff) | ((value & 7)<<8);
		env1Start = 1;
		pulse1Duty = 0;
		break;
	case 0x4004: /* Pulse 2 duty, envel., volume */
		pulse2Control = value;
		env2Divide = (pulse2Control&0xf);
		break;
	case 0x4005: /* Pulse 2 sweep, period, negate, shift */
		sweep2 = value;
		sweep2Divide = ((sweep2>>4)&7);
		sweep2Shift = (sweep2&7);
		sweep2Reload = 1;
		break;
	case 0x4006: /* Pulse 2 timer low */
			pulse2Timer = (pulse2Timer&0x700) | value;
		break;
	case 0x4007: /* Pulse 2 length counter, timer high */
		if (apuStatus & 2)
			pulse2Length = lengthTable[((value>>3)&0x1f)];
		pulse2Timer = (pulse2Timer&0xff) | ((value & 7)<<8);
		env2Start = 1;
		pulse2Duty = 0;
		break;
	case 0x4008: /* Triangle misc. */
			triControl = value;
		break;
	case 0x400a: /* Triangle timer low */
			triTimer = (triTimer&0x700) | value;
		break;
	case 0x400b: /* Triangle length, timer high */
		if (apuStatus & 4) {
			triLinReload = 1;
			triLength = lengthTable[((value>>3)&0x1f)];
		}
			triTimer = (triTimer&0xff) | ((value & 7)<<8);
		break;
	case 0x400c: /* Noise misc. */
		noiseControl = value;
		envNoiseDivide = (noiseControl&0xf);
		break;
	case 0x400e: /* Noise loop, period */
		noiseTimer = noiseTable[(value&0xf)];
		noiseMode = (value&0x80);
		break;
	case 0x400f: /* Noise length counter */
		if (apuStatus & 8) {
			noiseLength = lengthTable[((value>>3)&0x1f)];
			envNoiseStart = 1;
		}
		break;
	case 0x4010: /* DMC IRQ, loop, freq. */
		dmcControl = value;
		dmcRate = rateTable[(dmcControl&0xf)];
		dmcTemp = dmcRate;
		if (!(dmcControl&0x80)) {
			dmcInt = 0;
		}
		break;
	case 0x4011: /* DMC load counter */
		dmcOutput = (value&0x7f);
		break;
	case 0x4012: /* DMC sample address */
		dmcAddress = (0xc000 + (64 * value));
		dmcCurAdd = dmcAddress;
		break;
	case 0x4013: /* DMC sample length */
		dmcLength = ((16 * value) + 1);
		break;
	case 0x4014:
		source = ((value << 8) & 0xff00);
		if (cpucc%2) {
			apu_wait += 2;
			vdp_wait += 6;
			cpucc += 2;
		} else {
			apu_wait += 1;
			vdp_wait += 3;
			cpucc += 1;
		}
		run_ppu(vdp_wait);
		run_apu(apu_wait);
		for (int i = 0; i < 256; i++) {
			if (ppuOamAddress > 255)
				ppuOamAddress = 0;
			oam[ppuOamAddress] = cpuread(source++);
			ppuOamAddress++;
			apu_wait += 2;
			vdp_wait += 6;
			cpucc += 2;
			run_ppu(vdp_wait);
			run_apu(apu_wait);
		}
		break;
	case 0x4015: /* APU status */
		dmcInt = 0;
		apuStatus = value;
		if (!(apuStatus&0x01))
			pulse1Length = 0;
		if (!(apuStatus&0x02))
			pulse2Length = 0;
		if (!(apuStatus&0x04))
			triLength = 0;
		if (!(apuStatus&0x08))
			noiseLength = 0;
		if (!(apuStatus&0x10)) {
			dmcBytesLeft = 0;
			dmcSilence = 1;
		}
		else if (apuStatus&0x10) {
			if (!dmcBytesLeft)
				dmcRestart = 1;
		}
		break;
	case 0x4016:
		s = (value & 1);
		if (s == 1) {
			ctrb = 0;
			ctrb2 = 0;
		}
		break;
	case 0x4017: /* APU frame counter */
		frameWrite = 1;
		frameWriteDelay = 2+(apucc%2);
		apuFrameCounter = value;
		break;
	}
}

uint_fast8_t cpuread(uint16_t address) {
	uint_fast8_t value;
	if (address >= 0x8000)
		value = prgSlot[(address>>12)&~8][address&0xfff];
	else if (address >= 0x6000 && address < 0x8000) {
		if (wramEnable || extendedPrg)
		{
			value = wramSource[(address & 0x1fff)];
		}
		else if (wramBit)
		{
			value = (((address >> 4) & 0xfe) | wramBitVal);
		}
		else
			value = (address>>4); /* open bus */
	} else if (address < 0x2000)
		value = cpuRam[address & 0x7ff];
	else if (address >= 0x2000 && address < 0x4000)
		value = read_ppu_register(address);
	else if (address >= 0x4000 && address < 0x4020)
		value = read_cpu_register(address);
	else if (address >= 0x4020 && address < 0x6000)
		value = read_mapper_register(address);
	else {
		value = (address>>4); /* open bus */
	}
	return value;
}

void cpuwrite(uint16_t address, uint_fast8_t value) {
	if (address < 0x2000)
		cpuRam[address & 0x7ff] = value;
	else if (address >= 0x2000 && address < 0x4000)
		write_ppu_register(address, value);
	else if (address >= 0x4000 && address < 0x4020)
		write_cpu_register(address, value);
	else if (address >= 0x4020 && address < 0x6000)
		write_mapper_register4(address, value);
	else if (address >= 0x6000 && address < 0x8000)
	{
		write_mapper_register6(address, value);
		if (wramEnable && !extendedPrg)
			wramSource[(address & 0x1fff)] = value;
		else if (wramBit)
			wramBitVal = (value & 0x01);
	}
	else if (address >= 0x8000)
		write_mapper_register8(address, value);
}

void interrupt_handle(interrupt_t x) {
	dummy = cpuread(cpuPC);											/* cycle 2 */
		cpuwrite((0x100 + cpuS--), ((cpuPC) & 0xff00) >> 8);				/* cycle 3 */
		cpuwrite((0x100 + cpuS--), ((cpuPC) & 0xff));					/* cycle 4 */
		if (x == BRK) {
			cpuwrite((0x100 + cpuS--), (cpuP | 0x10)); /* set b flag */
		}
		else
			cpuwrite((0x100 + cpuS--), (cpuP & 0xef)); /* clear b flag */
																	/* cycle 5 */
		synchronize(3);
		interrupt_polling();
		if (nmiPending) {
			x = NMI;
			nmiPending = 0;
		}
		if (x == IRQ || x == BRK)
			cpuPC = (cpuread(irq + 1) << 8) + cpuread(irq);
		else
			cpuPC = (cpuread(nmi + 1) << 8) + cpuread(nmi);			/* cycle 6 (PCL) */
																	/* cycle 7 (PCH) */
		bitset(&cpuP, 1, 2); /* set I flag */

}

void power_reset () {
	init_mapper();
	/* same SP accesses as in the IRQ routine */
	cpuS--;
	cpuS--;
	cpuS--;
	cpuPC = (cpuread(rst + 1) << 8) + cpuread(rst);
	if (rstFlag == HARD_RESET) { /* TODO: what is correct behavior? */
		cpuwrite(0x4017, 0x00);
		apuStatus = 0; /* silence all channels */
		noiseShift = 1;
		dmcOutput = 0;
	}
	bitset(&cpuP, 1, 2); /* set I flag */
	rstFlag = NONE;
}

void interrupt_polling() {
	if (nmiFlipFlop && (nmiFlipFlop < (ppucc-1))) {
		nmiPending = 1;
		nmiFlipFlop = 0;
	}
	if (irqPulled && (!(cpuP & 0x04) || intDelay)) {
		irqPending = 1;
		irqPulled = 0;
	} else if ((cpuP & 0x04) && !intDelay) {
		irqPending = 0;
		irqPulled = 0;
	}
}

void synchronize(uint_fast8_t x) {
	apu_wait += addcycle;
	vdp_wait += addcycle * 3;
	cpucc += addcycle;
	addcycle = 0;
	run_ppu(vdp_wait-(x*3));
	run_apu(apu_wait-(x));
}
