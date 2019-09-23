#include "6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>	/* memcpy */
#include "../video/ppu.h"
#include "../audio/apu.h"
#include "../nes/mapper.h"
#include "../nes/nescartridge.h"
#include "../nes/nesemu.h"

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

static inline void power_reset(void), interrupt_polling();
static inline void accum(), immed(), zpage(), zpagex(), zpagey(), absol(), absxR(), absxW(), absyR(), absyW(), indx(), indyR(), indyW();
static inline void adc(), ahx(), alr(), and(), anc(), arr(), asl(), asli(), axs(), branch(), bit(), brkop(), clc(), cld(),
				   cli(), clv(), cmp(), cpx(), cpy(), dcp(), dec(), dex(), dey(), eor(), inc(), isc(), inx(), iny(), jmpa(), jmpi(), jsr(), las(),
				   lax(), lda(), ldx(), ldy(), lsr(), lsri(), nopop(), ora(), pha(), php(), pla(), plp(), rla(), rra(), rol(), roli(), ror(), rori(), rti(), rts(),
				   sax(), sbc(), sec(), sed(), sei(), shx(), shy(), slo(), sre(), sta(), stx(), sty(), tas(), tax(), tay(), tsx(), txa(), txs(),
				   tya(), xaa(), none();

uint_fast8_t mode, opcode, addmode, tmpval8, dummy, pcl, pch, dummywrite = 0, op, irqPulled = 0, nmiPulled = 0, irqPending = 0, nmiPending = 0, intDelay = 0;
uint16_t addr, tmpval16;

/* Mapped memory */
uint_fast8_t *prgSlot[0x8], cpuRam[0x800];

/* Internal registers */
uint_fast8_t cpuA = 0x00, cpuX = 0x00, cpuY = 0x00, cpuP = 0x00, cpuS = 0x00;
uint16_t cpuPC;
uint32_t cpucc = 0;

/* Vector pointers */
static const uint16_t nmi = 0xfffa, rst = 0xfffc, irq = 0xfffe;

void run_6502() {

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
		_6502_addcycles(7);
		interrupt_handle(NMI);
		nmiPending = 0;
		intDelay = 0;
	} else if (irqPending && !intDelay){
		_6502_addcycles(7);
		interrupt_handle(IRQ);
		irqPending = 0;
	}
	else{
		intDelay = 0;
		op = _6502_cpuread(cpuPC++);
		_6502_addcycles(ctable[op]);
		(*addtable[op])();
		(*optable[op])();
	}
	_6502_synchronize(0);
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
	_6502_synchronize(0);
}

void lax() {
	_6502_synchronize(0);
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
	dummy = _6502_cpuread(cpuPC);	/* cycle 2 */
}

void immed() {
	addr = cpuPC++;	/* cycle 2 */
}

void zpage() {
	addr = _6502_cpuread(cpuPC++);			/* cycle 2 */
}

void zpagex() {
	addr = _6502_cpuread(cpuPC++);			/* cycle 2 */
	dummy = _6502_cpuread(addr);
	addr = ((addr + cpuX) & 0xff);		/* cycle 3 */
}

void zpagey() {
	addr = _6502_cpuread(cpuPC++);			/* cycle 2 */
	dummy = _6502_cpuread(addr);
	addr = ((addr + cpuY) & 0xff);		/* cycle 3 */
}

void absol() {
	addr = _6502_cpuread(cpuPC++);			/* cycle 2 */
	addr += _6502_cpuread(cpuPC++) << 8;		/* cycle 3 */
}

void absxR() {
	pcl = _6502_cpuread(cpuPC++);					/* cycle 2 */
	pch = _6502_cpuread(cpuPC++);
	pcl += cpuX;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	if ((addr & 0xff) < cpuX) {				/* cycle 5 (optional) */
		dummy = _6502_cpuread(addr);
		addr += 0x100;
		_6502_addcycles(1);
	}
}

void absxW() {
	pcl = _6502_cpuread(cpuPC++);					/* cycle 2 */
	pch = _6502_cpuread(cpuPC++);
	pcl += cpuX;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	dummy = _6502_cpuread(addr);
	if ((addr & 0xff) < cpuX) {				/* cycle 5 (optional) */
		addr += 0x100;
		_6502_addcycles(1);
	}
}

void absyR() {
	pcl = _6502_cpuread(cpuPC++);					/* cycle 2 */
	pch = _6502_cpuread(cpuPC++);
	pcl += cpuY;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	if ((addr & 0xff) < cpuY) {				/* cycle 5 (optional) */
		dummy = _6502_cpuread(addr);
		addr += 0x100;
		_6502_addcycles(1);
	}
}

void absyW() {
	pcl = _6502_cpuread(cpuPC++);					/* cycle 2 */
	pch = _6502_cpuread(cpuPC++);
	pcl += cpuY;								/* cycle 3 */
	addr = ((pch << 8) | pcl);
	dummy = _6502_cpuread(addr);
	if ((addr & 0xff) < cpuY) {				/* cycle 5 (optional) */
		addr += 0x100;
		_6502_addcycles(1);
	}
}

void indx() {
	tmpval8 = _6502_cpuread(cpuPC++);						/* cycle 2 */
	dummy = _6502_cpuread(tmpval8);						/* cycle 3 */
	pcl = _6502_cpuread(((tmpval8+cpuX) & 0xff));			/* cycle 4 */
	pch = _6502_cpuread(((tmpval8+cpuX+1) & 0xff));			/* cycle 5 */
	addr = ((pch << 8) | pcl);						/* cycle 6 */
}

void indyR() {
	tmpval8 = _6502_cpuread(cpuPC++);						/* cycle 2 */
	pcl = _6502_cpuread(tmpval8++);						/* cycle 3 */
	pch = _6502_cpuread((tmpval8 & 0xff));
	pcl += cpuY;										/* cycle 4 */
	addr = ((pch << 8) | pcl);						/* cycle 5 */
	if ((addr & 0xff) < cpuY) {						/* cycle 6 (optional) */
		dummy = _6502_cpuread(addr);
		addr += 0x100;
		_6502_addcycles(1);
	}
}

void indyW() {
	tmpval8 = _6502_cpuread(cpuPC++);						/* cycle 2 */
	pcl = _6502_cpuread(tmpval8++);						/* cycle 3 */
	pch = _6502_cpuread((tmpval8 & 0xff));
	pcl += cpuY;										/* cycle 4 */
	addr = ((pch << 8) | pcl);						/* cycle 5 */
	dummy = _6502_cpuread(addr);
	if ((addr & 0xff) < cpuY) {						/* cycle 6 (optional) */
		addr += 0x100;
		_6502_addcycles(1);
	}
}

		/* OPCODES */
void adc() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	tmpval16 = cpuA + tmpval8 + (cpuP & 1);
	bitset(&cpuP, (cpuA ^ tmpval16) & (tmpval8 ^ tmpval16) & 0x80, 6);
	bitset(&cpuP, tmpval16 > 0xff, 0);
	cpuA = tmpval16;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void and() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	cpuA &= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void asl() {
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);			/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);				/* cycle 5 */
	bitset(&cpuP, tmpval8 & 0x80, 0);
	tmpval8 = tmpval8 << 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);								/* cycle 6 */
	dummywrite = 0;
}

void asli() {
	_6502_synchronize(1);
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
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, !(cpuA & tmpval8), 1);
	bitset(&cpuP, tmpval8 & 0x80, 7);
	bitset(&cpuP, tmpval8 & 0x40, 6);
}

int pageCross;
void branch() {
	_6502_synchronize(1);
	interrupt_polling();
	uint_fast8_t reflag[4] = { 7, 6, 0, 1 };
	/* fetch operand */											/* cycle 2 */
	if (((cpuP >> reflag[(op >> 6) & 3]) & 1) == ((op >> 5) & 1)) {
		if (((cpuPC + 1) & 0xff00)	!= ((cpuPC + ((int8_t) _6502_cpuread(cpuPC) + 1)) & 0xff00)) {
			_6502_addcycles(1);
			/* correct? */
			_6502_synchronize(1);
			interrupt_polling();
			pageCross = 1;
		}
		else
			pageCross = 0;
		/* prefetch next opcode, optionally add operand to pc*/	/* cycle 3 (branch) */
		cpuPC = cpuPC + (int8_t) _6502_cpuread(cpuPC) + 1;

		/* fetch next opcode if branch taken, fix PCH */		/* cycle 4 (optional) */
		/* fetch opcode if page boundary */						/* cycle 5 (optional) */
		_6502_addcycles(1);
		if (pageCross) /* special case, non-page crossing + branch taking ignores int. */
		{
		_6502_synchronize(1);
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
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 0, 0);
}

void cld() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 0, 3);
}

void cli() {
	_6502_synchronize(1); /* delay interrupt if happen here */
	intDelay = 1;
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 0, 2);
}

void clv() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 0, 6);
}

void cmp() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, (cpuA - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuA == tmpval8, 1);
	bitset(&cpuP, cpuA >= tmpval8, 0);
}

void cpx() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);
	bitset(&cpuP, (cpuX - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuX == tmpval8, 1);
	bitset(&cpuP, cpuX >= tmpval8, 0);
}

void cpy() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);
	bitset(&cpuP, (cpuY - tmpval8) & 0x80, 7);
	bitset(&cpuP, cpuY == tmpval8, 1);
	bitset(&cpuP, cpuY >= tmpval8, 0);
}

/* DCP (r-m-w) */

void dec() {
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);					/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);						/* cycle 5 */
	tmpval8 = tmpval8-1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);									/* cycle 6 */
	dummywrite = 0;
}

void dex() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuX--;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void dey() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuY--;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void eor() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);								/* cycle 4 */
	cpuA ^= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void inc() {
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);				/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);					/* cycle 5 */
	tmpval8 = tmpval8 + 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);					/* cycle 6 */
	dummywrite = 0;
}

void inx() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuX++;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void iny() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuY++;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

/* ISB (r-m-w) */

void jmpa() {
	addr = _6502_cpuread(cpuPC++);			/* cycle 2 */
	_6502_synchronize(1);
	interrupt_polling();
	addr += _6502_cpuread(cpuPC++) << 8;		/* cycle 3 */
	cpuPC = addr;
}

void jmpi() {
	tmpval8 = _6502_cpuread(cpuPC++);								/* cycle 2 */
	tmpval16 = (_6502_cpuread(cpuPC) << 8);							/* cycle 3 */
	addr = _6502_cpuread(tmpval16 | tmpval8);					/* cycle 4 */
	_6502_synchronize(1);
	interrupt_polling();
	addr += _6502_cpuread(tmpval16 | ((tmpval8+1) & 0xff)) << 8;	/* cycle 5 */
	cpuPC = addr;
}

void jsr() {
	_6502_cpuwrite((0x100 + cpuS--), ((cpuPC + 1) & 0xff00) >> 8);	/* cycle 4 */
	_6502_cpuwrite((0x100 + cpuS--), ((cpuPC + 1) & 0x00ff));		/* cycle 5 */
	addr = _6502_cpuread(cpuPC++);								/* cycle 2 */
	/* internal operation? */							/* cycle 3 */
	_6502_synchronize(1);
	interrupt_polling();
	addr += _6502_cpuread(cpuPC) << 8;							/* cycle 6 */
	cpuPC = addr;
}

/* LAX (read instruction) */

void lda() {
	_6502_synchronize(1);
	interrupt_polling();
	cpuA = _6502_cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void ldx() {
	_6502_synchronize(1);
	interrupt_polling();
	cpuX = _6502_cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void ldy() {
	_6502_synchronize(1);
	interrupt_polling();
	cpuY = _6502_cpuread(addr);						/* cycle 4 */
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void lsr() {
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);			/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);				/* cycle 5 */
	bitset(&cpuP, tmpval8 & 1, 0);
	tmpval8 = tmpval8 >> 1;
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);				/* cycle 6 */
	dummywrite = 0;
}

void lsri() {
	_6502_synchronize(1);
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
	_6502_synchronize(1);
	interrupt_polling();
}

void ora() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	cpuA |= tmpval8;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void pha() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);			/* cycle 2 */
	_6502_cpuwrite((0x100 + cpuS--), cpuA);		/* cycle 3 */
}

void php() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);			/* cycle 2 */
	_6502_cpuwrite((0x100 + cpuS--), (cpuP | 0x30)); /* bit 4 is set if from an instruction */
}									/* cycle 3 */

void pla() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);			/* cycle 2 */
	/* inc sp */					/* cycle 3 */
	cpuA = _6502_cpuread(++cpuS + 0x100);		/* cycle 4 */
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void plp() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);			/* cycle 2 */
	/* inc sp */					/* cycle 3 */
	cpuP = _6502_cpuread(++cpuS + 0x100);	/* cycle 4 */
	bitset(&cpuP, 1, 5);
	bitset(&cpuP, 0, 4); /* b flag should be discarded */
}

/* RLA (r-m-w) */

void rol() {
	uint8_t bkp;
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);			/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);				/* cycle 5 */
	bkp = tmpval8;
	tmpval8 = tmpval8 << 1;
	bitset(&tmpval8, cpuP & 1, 0);
	bitset(&cpuP, bkp & 0x80, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);				/* cycle 6 */
	dummywrite = 0;
}

void roli() {
	_6502_synchronize(1);
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
	uint8_t bkp;
	_6502_synchronize(2);
	tmpval8 = _6502_cpuread(addr);					/* cycle 4 */
	_6502_cpuwrite(addr,tmpval8);						/* cycle 5 */
	bkp = tmpval8;
	tmpval8 = tmpval8 >> 1;
	bitset(&tmpval8, cpuP & 1, 7);
	bitset(&cpuP, bkp & 1, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	_6502_synchronize(1);
	interrupt_polling();
	dummywrite = 1;
	_6502_cpuwrite(addr,tmpval8);						/* cycle 6 */
	dummywrite = 0;
}

void rori() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = cpuA;					/* cycle 4 */
	cpuA = tmpval8;						/* cycle 5 */
	tmpval8 >>= 1;
	bitset(&tmpval8, cpuP & 1, 7);
	bitset(&cpuP, cpuA & 1, 0);
	bitset(&cpuP, tmpval8 == 0, 1);
	bitset(&cpuP, tmpval8 >= 0x80, 7);
	cpuA = tmpval8;								/* cycle 6 */
}

/* RRA (r-m-w) */

void rti() {
	dummy = _6502_cpuread(cpuPC);					/* cycle 2 */
	/* stack inc */							/* cycle 3 */
	cpuP = _6502_cpuread(++cpuS + 0x100);			/* cycle 4 */
	bitset(&cpuP, 1, 5); /* bit 5 always set */
	bitset(&cpuP, 0, 4); /* b flag should be discarded */
	cpuPC = _6502_cpuread(++cpuS + 0x100);			/* cycle 5 */
	_6502_synchronize(1);
	interrupt_polling();
	cpuPC += (_6502_cpuread(++cpuS + 0x100) << 8);	/* cycle 6 */
}

void rts() {
	dummy = _6502_cpuread(cpuPC);					/* cycle 2 */
	/* stack inc */							/* cycle 3 */
	addr = _6502_cpuread(++cpuS + 0x100);			/* cycle 4 */
	addr += _6502_cpuread(++cpuS + 0x100) << 8;	/* cycle 5 */
	_6502_synchronize(1);
	interrupt_polling();
	cpuPC = addr + 1;							/* cycle 6 */
}

/* SAX (write instruction) */

void sbc() {
	_6502_synchronize(1);
	interrupt_polling();
	tmpval8 = _6502_cpuread(addr);						/* cycle 4 */
	tmpval16 = cpuA + (tmpval8 ^ 0xff) + (cpuP & 1);
	bitset(&cpuP, (cpuA ^ tmpval16) & (tmpval8 ^ cpuA) & 0x80, 6);
	bitset(&cpuP, tmpval16 > 0xff, 0);
	cpuA = tmpval16;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void sec() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 1, 0);
}

void sed() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 1, 3);
}

void sei() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	bitset(&cpuP, 1, 2);
}

/* SLO (r-m-w) */

/* SRE (r-m-w) */

void sta() {
	tmpval8 = cpuA;
	_6502_synchronize(1);
	interrupt_polling();
	_6502_cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void stx() {
	tmpval8 = cpuX;
	_6502_synchronize(1);
	interrupt_polling();
	_6502_cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void sty() {
	tmpval8 = cpuY;
	_6502_synchronize(1);
	interrupt_polling();
	_6502_cpuwrite(addr,tmpval8);				/* cycle 4 */
}

void tax() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuX = cpuA;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void tay() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuY = cpuA;
	bitset(&cpuP, cpuY == 0, 1);
	bitset(&cpuP, cpuY >= 0x80, 7);
}

void tsx() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuX = cpuS;
	bitset(&cpuP, cpuX == 0, 1);
	bitset(&cpuP, cpuX >= 0x80, 7);
}

void txa() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuA = cpuX;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void txs() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuS = cpuX;
}

void tya() {
	_6502_synchronize(1);
	interrupt_polling();
	dummy = _6502_cpuread(cpuPC);
	cpuA = cpuY;
	bitset(&cpuP, cpuA == 0, 1);
	bitset(&cpuP, cpuA >= 0x80, 7);
}

void none() {}

void interrupt_handle(interrupt_t x) {
	dummy = _6502_cpuread(cpuPC);											/* cycle 2 */
		_6502_cpuwrite((0x100 + cpuS--), ((cpuPC) & 0xff00) >> 8);				/* cycle 3 */
		_6502_cpuwrite((0x100 + cpuS--), ((cpuPC) & 0xff));					/* cycle 4 */
		if (x == BRK) {
			_6502_cpuwrite((0x100 + cpuS--), (cpuP | 0x10)); /* set b flag */
		}
		else
			_6502_cpuwrite((0x100 + cpuS--), (cpuP & 0xef)); /* clear b flag */
																	/* cycle 5 */
		_6502_synchronize(3);
		interrupt_polling();
		if (nmiPending) {
			x = NMI;
			nmiPending = 0;
		}
		if (x == IRQ || x == BRK)
			cpuPC = (_6502_cpuread(irq + 1) << 8) + _6502_cpuread(irq);
		else
			cpuPC = (_6502_cpuread(nmi + 1) << 8) + _6502_cpuread(nmi);			/* cycle 6 (PCL) */
																	/* cycle 7 (PCH) */
		bitset(&cpuP, 1, 2); /* set I flag */

}

void power_reset () {
	init_mapper();
	/* same SP accesses as in the IRQ routine */
	cpuS--;
	cpuS--;
	cpuS--;
	cpuPC = (_6502_cpuread(rst + 1) << 8) + _6502_cpuread(rst);
	if (rstFlag == HARD_RESET) { /* TODO: what is correct behavior? */
		_6502_cpuwrite(0x4017, 0x00);
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
