/*Zilog Z80 microprocessor
 * This is a software compatible extension of the Intel 8080
 *
 * Detailed documentation of instructions and their bus responses cycle for cycle: http://baltazarstudios.com/zilog-z80-undocumented-behavior/
 *
 * TODO:
 * -emulate wait states
 * -emulate the different interrupt modes
 * -all (undocumented) opcodes
 * -better synchronization - all writes to (HL); I/O read/write...
 */

#include "z80.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "smsemu.h"

#define S_SHIFT		7
#define Z_SHIFT		6
#define Y_SHIFT		5
#define H_SHIFT		4
#define X_SHIFT		3
#define P_SHIFT		2
#define N_SHIFT		1
#define C_SHIFT		0
#define S_FLAG 		(1 << S_SHIFT)
#define Z_FLAG  	(1 << Z_SHIFT)
#define Y_FLAG  	(1 << Y_SHIFT)
#define H_FLAG  	(1 << H_SHIFT)
#define X_FLAG  	(1 << X_SHIFT)
#define P_FLAG  	(1 << P_SHIFT)
#define N_FLAG  	(1 << N_SHIFT)
#define C_FLAG  	(1 << C_SHIFT)
#define YX_FLAG 	(Y_FLAG | X_FLAG)
#define YXC_FLAG 	(YX_FLAG | C_FLAG)
#define SZYXC_FLAG	(S_FLAG | Z_FLAG | YXC_FLAG)
#define SZYXP_FLAG	(S_FLAG | Z_FLAG | YX_FLAG | P_FLAG)
#define SZYXPC_FLAG	(SZYXP_FLAG | C_FLAG)

static const uint_fast8_t ctable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4,/* 0 */
	8,10, 7, 6, 4, 4, 7, 4,12,11, 7, 6, 4, 4, 7, 4,/* 1 */
	7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4,/* 2 */
	7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4,/* 3 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 4 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 5 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 6 */
	7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4,/* 7 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 8 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* 9 */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* a */
	4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,/* b */
	5,10,10,10,10,11, 7,11, 5,10,10, 0,10,17, 7,11,/* c */
	5,10,10,11,10,11, 7,11, 5, 4,10,11,10, 0, 7,11,/* d */
	5,10,10,19,10,11, 7,11, 5, 4,10, 4,10, 0, 7,11,/* e */
	5,10,10, 4,10,11, 7,11, 5, 6,10, 4,10, 0, 7,11,/* f */
};
static const uint_fast8_t ccbtable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 0 */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 1 */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 2 */
	8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 3 */
    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 4 */
    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 5 */
    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 6 */
    8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,/* 7 */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 8 */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* 9 */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* a */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* b */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* c */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* d */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* e */
    8, 8, 8, 8, 8, 8,15, 8, 8, 8, 8, 8, 8, 8,15, 8,/* f */
};
static const uint_fast8_t cddtable[] = {
 /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	4,99,99,99,99,99,99,99,99,15,99,99,99,99,99, 4,/* 0 */
	4,99,99,99,99,99,99,99,99,15,99,99,99,99,99, 4,/* 1 */
	4,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11, 4,/* 2 */
	4,99,99,99,23,23,19,99,99,15,99,99,99,99,99, 4,/* 3 */
	4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 4 */
	4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 5 */
	8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
   19,19,19,19,19,19,99,19, 4, 4, 4, 4, 8, 8,19, 4,/* 7 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* 8 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* 9 */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* a */
	4,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19, 4,/* b */
	4,99,99,99,99,99,99,99,99,99,99, 0,99,99,99, 4,/* c */
	4,99,99,99,99,99,99,99,99,99,99,99,99, 0,99, 4,/* d */
	4,14,99,23,99,15,99,99,99, 8,99,99,99, 0,99, 4,/* e */
	4,99,99,99,99,99,99,99,99,10,99,99,99, 0,99, 4,/* f */
};
static const uint_fast8_t cedtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 0 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 1 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 2 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 3 */
	12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,/* 4 */
	12,12,15,20, 8,14, 8, 9,12,12,15,20, 8,14, 8, 9,/* 5 */
	12,12,15,20, 8,14, 8,18,12,12,15,20, 8,14, 8,18,/* 6 */
	12,12,15,20,99,99,99,99,12,12,15,20,99,99,99,99,/* 7 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
	16,16,16,16,99,99,99,99,16,16,16,16,99,99,99,99,/* a */
	16,16,16,16,99,99,99,99,16,16,16,16,99,99,99,99,/* b */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
};
static const uint_fast8_t cfdtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,04,/* 0 */
	99,99,99,99,99,99,99,99,99,15,99,99,99,99,99,04,/* 1 */
	99,14,20,10, 8, 8,11,99,99,15,20,10, 8, 8,11,04,/* 2 */
	99,99,99,99,23,23,19,99,99,15,99,99,99,99,99,04,/* 3 */
	 4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 4 */
	 4, 4, 4, 4, 8, 8,19, 4, 4, 4, 4, 4, 8, 8,19, 4,/* 5 */
	 8, 8, 8, 8, 8, 8,19, 8, 8, 8, 8, 8, 8, 8,19, 8,/* 6 */
	19,19,19,19,19,19, 4,19, 4, 4, 4, 4, 8, 8,19, 4,/* 7 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* 8 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* 9 */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* a */
	99,99,99,99, 8, 8,19,99,99,99,99,99, 8, 8,19,04,/* b */
	99,99,99,99,99,99,99,99,99,99,99, 0,04,04,04,04,/* c */
	99,99,99,99,99,99,99,99,99,99,99,99,04,04,04,04,/* d */
	99,14,99,23,99,15,99,99,99, 8,99,99,04,04,04,04,/* e */
	99,99,99,99,99,99,99,99,99,10,99,99,04,04,04,04,/* f */
};
static const uint_fast8_t cddcbtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 1 */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 2 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,23,99,/* 3 */
	99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 4 */
	99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 5 */
	99,99,99,99,99,99,20, 8, 8, 8, 8, 8, 8, 8,20, 8,/* 6 */
	99,99,99,99,99,99,20,19,99,99,99,99, 8, 8,20,99,/* 7 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* a */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* b */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
};
static const uint_fast8_t cfdcbtable[] = {
  /*0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f       */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 0 */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 1 */
	99,99,99,99,99,99,23,99,99,99,99,99,99,99,23,99,/* 2 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,23,99,/* 3 */
	99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 4 */
	99,99,99,99,99,99,20,99,99,99,99,99,99,99,20,99,/* 5 */
	99,99,99,99,99,99,20, 8, 8, 8, 8, 8, 8, 8,20, 8,/* 6 */
	99,99,99,99,99,99,20,19,99,99,99,99, 8, 8,20,99,/* 7 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 8 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* 9 */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* a */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* b */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* c */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* d */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* e */
	99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,/* f */
};

static inline void interrupt_polling(), noop(), unp();
static inline uint8_t parcalc(uint8_t);

/*OPCODE FUNCTIONS*/

/* 8-bit load */
static inline void ld(), ldim(), ldixi(), ldiyi(), ldiix(), ldiiy(), ldixn(), ldiyn(), ldabc(), ldade(), ldai(), ldbca(), lddea(), ldia(), ldain(), ldar(), ldina(), ldra(), lhlim(), ldhlr();
/* 16-bit load */
static inline void ld16(), ldix(), ldiy(), ldhli(), ldrp(), ldxin(), ldyin(), ldihl(), ldidd(), linix(), liniy(), lsphl(), lspix(), lspiy(), push(), pusix(), pusiy(), pop(), popix(), popiy();
/* exchange, block transfer, search */
static inline void ex(), exaf(), exx(), exhl(), exix(), exiy(), ldi(), ldir(), ldd(), lddr(), cpi(), cpir(), cpd(), cpdr();
/* 8-bit arithmetic */
static inline void add(uint8_t), addr(), addi(), adixh(), adixl(), adiyh(), adiyl(), adixi(), adiyi(), adc(uint8_t), adcr(), adci(), acixh(), acixl(), aciyh(), aciyl(), acixi(), aciyi(), sub(uint8_t), subr(), subi(), sbixh(), sbixl(), sbiyh(), sbiyl(), sbixi(), sbiyi(), sbc(uint8_t), sbcr(), sbci(), scixh(), scixl(), sciyh(), sciyl(), scixi(), sciyi(), inchl(), dechl();
static inline void and(uint8_t), andr(), andi(), anixh(), anixl(), aniyh(), aniyl(), anixi(), aniyi(),  or(uint8_t), orr(), ori(), orixh(), orixl(), oriyh(), oriyl(), orixi(), oriyi(), xor(uint8_t), xorr(), xori(), xrixh(), xrixl(), xriyh(), xriyl(), xrixi(), xriyi(), cp(uint8_t), cpr(), cpn(), cpixh(), cpixl(), cpiyh(), cpiyl(), cpixi(), cpiyi(), inc(), inixi(), iniyi(), dec(), deixi(), deiyi();
/* general purpose */
static inline void daa(), cpl(), neg(), ccf(), scf(), nop(), halt(), di(), ei(), im();
/* 16-bit arithmetic */
static inline void addrp(), adcrp(), sbcrp(), addix(), addiy(), incrp(), incix(), inciy(), decrp(), decix(), deciy();
/* rotate, shift */
static inline void rlca(), rla(), rrca(), rra(), rlc(), rlcix(), rlciy(), rl(), rlix(), rliy(), rrc(), rrcix(), rrciy(), rr(), rrix(), rriy(), sla(), slaix(), slaiy(), sra(), sraix(), sraiy(), sll(), sllix(), slliy(), srl(), srlix(), srliy(), rld(), rrd();
/* bit set, reset, test */
static inline void bit(), bitix(), bitiy(), set(), setix(), setiy(), res(), resix(), resiy();
/* jump */
static inline void jp(), jpc(), jr(), jrc(), jphl(), jpix(), jpiy(), djnz();
/* call, return */
static inline void call(), callc(), ret(), retc(), reti(), retn(), rst();
/* input, output */
static inline void in(), inrc(), ini(), inir(), out(), outc(), outi(), otir(), outd(), otdr();
/* prefixed opcodes */
static inline void cb(), dd(), ed(), fd(), ddcb(), fdcb();
/* undocumented opcodes */
static inline void dcixh(), dciyh(), dcixl(), dciyl(), inixh(), iniyh(), inixl(), iniyl(), ldixh(), ldiyh(), ldixl(), ldiyl(), lrixh(), lrixl(), lriyh(), lriyl(), lixhr(), lixlr(), liyhr(), liylr();

static int8_t displace;//int since offset can be both positive and negative
static uint8_t op, intDelay = 0, halted = 0;

// Globals
uint8_t irqPulled = 0, nmiPulled = 0;

/* Internal registers */
static uint16_t cpuAF, cpuAFx, cpuBC, cpuDE, cpuHL, cpuBCx, cpuDEx, cpuHLx;
static uint8_t cpuI, cpuR; /* special purpose (8-bit) */
static uint16_t cpuIX, cpuIY, cpuPC, cpuSP; /* special purpose (16-bit) */
static uint8_t *cpuAreg, *cpuFreg, *cpuBreg, *cpuCreg, *cpuDreg, *cpuEreg, *cpuHreg, *cpuLreg, *cpuIXhreg, *cpuIXlreg, *cpuIYhreg, *cpuIYlreg;
static uint16_t *cpuAFreg, *cpuBCreg, *cpuDEreg, *cpuHLreg, *cpuIXreg, *cpuIYreg;

/* Interrupt flip-flops */
static uint8_t iff1, iff2, iMode;

/* Vector pointers */
static const uint16_t nmi = 0x66, irq = 0x38;

static uint8_t *r[8];
static uint16_t *rp[4], *rp2[4];

char opmess[] = "Unimplemented opcode";
void run_z80(){

static void (*optable[0x100])() = {
  /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	  nop, ld16,ldbca,incrp,  inc,  dec, ldim, rlca, exaf,addrp,ldabc,decrp,  inc,  dec, ldim, rrca, /* 0 */
	 djnz, ld16,lddea,incrp,  inc,  dec, ldim,  rla,   jr,addrp,ldade,decrp,  inc,  dec, ldim,  rra, /* 1 */
	  jrc, ld16,ldihl,incrp,  inc,  dec, ldim,  daa,  jrc,addrp,ldhli,decrp,  inc,  dec, ldim,  cpl, /* 2 */
	  jrc, ld16, ldia,incrp,inchl,dechl,lhlim,  scf,  jrc,addrp, ldai,decrp,  inc,  dec, ldim,  ccf, /* 3 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 4 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 5 */
	   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 6 */
	ldhlr,ldhlr,ldhlr,ldhlr,ldhlr,ldhlr, halt,ldhlr,   ld,   ld,   ld,   ld,   ld,   ld,   ld,   ld, /* 7 */
	 addr, addr, addr, addr, addr, addr, addr, addr, adcr, adcr, adcr, adcr, adcr, adcr, adcr, adcr, /* 8 */
	 subr, subr, subr, subr, subr, subr, subr, subr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, sbcr, /* 9 */
	 andr, andr, andr, andr, andr, andr, andr, andr, xorr, xorr, xorr, xorr, xorr, xorr, xorr, xorr, /* a */
	  orr,  orr,  orr,  orr,  orr,  orr,  orr,  orr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr,  cpr, /* b */
	 retc,  pop,  jpc,   jp,callc, push, addi,  rst, retc,  ret,  jpc,   cb,callc, call, adci,  rst, /* c */
	 retc,  pop,  jpc,  out,callc, push, subi,  rst, retc,  exx,  jpc,   in,callc,   dd, sbci,  rst, /* d */
	 retc,  pop,  jpc, exhl,callc, push, andi,  rst, retc, jphl,  jpc,   ex,callc,   ed, xori,  rst, /* e */
	 retc,  pop,  jpc,   di,callc, push,  ori,  rst, retc,lsphl,  jpc,   ei,callc,   fd,  cpn,  rst, /* f */
};
if((irqPulled && iff1 && !intDelay) || nmiPulled){
		if(halted){
			halted = 0;
			cpuPC++;
		}
		/* TODO: this is assuming Mode 1 */
		write_z80_memory(--cpuSP, ((cpuPC & 0xff00) >> 8));
		write_z80_memory(--cpuSP, ( cpuPC & 0x00ff));
		if (irqPulled){
			iff1 = iff2 = 0;
			addcycles(13);
			irqPulled = 0;
			cpuPC = irq;
		}
		else if (nmiPulled){
			iff2 = iff1;
			iff1 = 0;
			addcycles(11);
			nmiPulled = 0;
			cpuPC = nmi;
		}
	}
	else {
		intDelay = 0;
		op = *read_z80_memory(cpuPC++);
		cpuR = ((cpuR & 0x80) | ((cpuR & 0x7f) + 1));
		addcycles(ctable[op]);
		//fprintf(logfile,"%02x\t%04x\t%04x\n",op,cpuPC-1,cpuSP);
		//if(cpuPC == 0xdf91)
		//	exit(1);
		(*optable[op])();
	}
synchronize(0);
}

/* EXTENDED OPCODE TABLES */
void cb(){
static void (*cbtable[0x100])() = {
/*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rlc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc,  rrc, /* 0 */
	 rl,   rl,   rl,   rl,   rl,   rl,   rl,   rl,   rr,   rr,   rr,   rr,   rr,   rr,   rr,   rr, /* 1 */
	sla,  sla,  sla,  sla,  sla,  sla,  sla,  sla,  sra,  sra,  sra,  sra,  sra,  sra,  sra,  sra, /* 2 */
	sll,  sll,  sll,  sll,  sll,  sll,  sll,  sll,  srl,  srl,  srl,  srl,  srl,  srl,  srl,  srl, /* 3 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 4 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 5 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 6 */
	bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit,  bit, /* 7 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* 8 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* 9 */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* a */
	res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res,  res, /* b */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* c */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* d */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* e */
	set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set,  set, /* f */
	};
op = *read_z80_memory(cpuPC++);
addcycles(ccbtable[op]);
(*cbtable[op])();
}
void dd(){
static void (*ddtable[0x100])() = {
/*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 0 */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 1 */
	unp, ldix,linix,incix,inixh,dcixh,ldixh, noop, noop,addix,ldxin,decix,inixl,dcixl,ldixl, noop, /* 2 */
	unp, noop, noop, noop,inixi,deixi,ldixn, noop, noop,addix, noop, noop, noop, noop, noop, noop, /* 3 */
	unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 4 */
	unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 5 */
  lixhr,lixhr,lixhr,lixhr,lrixh,lrixl,ldixi,lixhr,lixlr,lixlr,lixlr,lixlr,lrixh,lrixl,ldixi,lixlr, /* 6 */
  ldiix,ldiix,ldiix,ldiix,ldiix,ldiix,  unp,ldiix,  unp,  unp,  unp,  unp,lrixh,lrixl,ldixi,  unp, /* 7 */
	unp, noop, noop, noop,adixh,adixh,adixi, noop, noop, noop, noop, noop,acixh,acixl,acixi, noop, /* 8 */
	unp, noop, noop, noop,sbixh,sbixl,sbixi, noop, noop, noop, noop, noop,scixh,scixl,scixi, noop, /* 9 */
	unp, noop, noop, noop,anixh,anixl,anixi, noop, noop, noop, noop, noop,xrixh,xrixl,xrixi, noop, /* a */
	unp, noop, noop, noop,orixh,orixl,orixi, noop, noop, noop, noop, noop,cpixh,cpixl,cpixi, noop, /* b */
	unp, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, ddcb, noop, noop, noop, noop, /* c */
	unp, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop,  nop, noop, noop, /* d */
	unp,popix, noop, exix, noop,pusix, noop, noop, noop, jpix, noop, noop, noop,  nop, noop, noop, /* e */
	unp, noop, noop, noop, noop, noop, noop, noop, noop,lspix, noop, noop, noop,  nop, noop, noop, /* f */
	};
op = *read_z80_memory(cpuPC++);
addcycles(cddtable[op]);
(*ddtable[op])();
}
void ed(){
static void (*edtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 0 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 1 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 2 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 3 */
	inrc, outc,sbcrp,ldidd,  neg, retn,   im,ldina, inrc, outc,adcrp, ldrp,  neg, reti, noop, ldra, /* 4 */
	inrc, outc,sbcrp,ldidd,  neg, noop,   im,ldain, inrc, outc,adcrp, ldrp,  neg, noop, noop, ldar, /* 5 */
	inrc, outc,sbcrp,ldidd,  neg, noop,   im,  rrd, inrc, outc,adcrp, ldrp,  neg, noop, noop,  rld, /* 6 */
	inrc, outc,sbcrp,ldidd,  neg, noop,   im, noop, inrc, outc,adcrp, ldrp,  neg, noop, noop, noop, /* 7 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 8 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* 9 */
	 ldi,  cpi,  ini, outi, noop, noop, noop, noop,  ldd,  cpd, noop, outd, noop, noop, noop, noop, /* a */
	ldir, cpir, inir, otir, noop, noop, noop, noop, lddr, cpdr, noop, otdr, noop, noop, noop, noop, /* b */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* c */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* d */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* e */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, /* f */
	};
op = *read_z80_memory(cpuPC++);
addcycles(cedtable[op]);
(*edtable[op])();
}
void fd(){
static void (*fdtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 0 */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 1 */
	noop, ldiy,liniy,inciy,iniyh,dciyh,ldiyh, noop, noop,addiy,ldyin,deciy,iniyl,dciyl,ldiyl, unp, /* 2 */
	noop, noop, noop, noop,iniyi,deiyi,ldiyn, noop, noop,addiy, noop, noop, noop, noop, noop, unp, /* 3 */
	 unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 4 */
	 unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 5 */
   liyhr,liyhr,liyhr,liyhr,liyhr,liyhr,ldiyi,liyhr,liylr,liylr,liylr,liylr,liylr,liylr,ldiyi,liylr, /* 6 */
   ldiiy,ldiiy,ldiiy,ldiiy,ldiiy,ldiiy,  unp,ldiiy,  unp,  unp,  unp,  unp,lriyh,lriyl,ldiyi,  unp, /* 7 */
    noop, noop, noop, noop,adiyh,adiyl,adiyi, noop, noop, noop, noop, noop,aciyh,aciyl,aciyi, unp, /* 8 */
	noop, noop, noop, noop,sbiyh,sbiyl,sbiyi, noop, noop, noop, noop, noop,sciyh,sciyl,sciyi, unp, /* 9 */
	noop, noop, noop, noop,aniyh,aniyl,aniyi, noop, noop, noop, noop, noop,xriyh,xriyl,xriyi, unp, /* a */
	noop, noop, noop, noop,oriyh,oriyl,oriyi, noop, noop, noop, noop, noop,cpiyh,cpiyl,cpiyi, unp, /* b */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, fdcb, unp, unp, unp, unp, /* c */
	noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, noop, unp, unp, unp, unp, /* d */
	noop,popiy, noop, exiy, noop,pusiy, noop, noop, noop, jpiy, noop, noop, unp, unp, unp, unp, /* e */
	noop, noop, noop, noop, noop, noop, noop, noop, noop,lspiy, noop, noop, unp, unp, unp, unp, /* f */
	};
op = *read_z80_memory(cpuPC++);
addcycles(cfdtable[op]);
(*fdtable[op])();
}

void ddcb(){
  static void (*ddcbtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop,rlcix, noop, noop, noop, noop, noop, noop, noop,rrcix, noop, /* 0 */
	noop, noop, noop, noop, noop, noop, rlix, noop, noop, noop, noop, noop, noop, noop, rrix, noop, /* 1 */
	noop, noop, noop, noop, noop, noop,slaix, noop, noop, noop, noop, noop, noop, noop,sraix, noop, /* 2 */
	noop, noop, noop, noop, noop, noop,sllix, noop, noop, noop, noop, noop, noop, noop,srlix, noop, /* 3 */
	noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 4 */
	noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 5 */
	noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 6 */
	noop, noop, noop, noop, noop, noop,bitix, noop, noop, noop, noop, noop, noop, noop,bitix, noop, /* 7 */
	noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* 8 */
	noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* 9 */
	noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* a */
	noop, noop, noop, noop, noop, noop,resix, noop, noop, noop, noop, noop, noop, noop,resix, noop, /* b */
	noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* c */
	noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* d */
	noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* e */
	noop, noop, noop, noop, noop, noop,setix, noop, noop, noop, noop, noop, noop, noop,setix, noop, /* f */
	};
	displace = *read_z80_memory(cpuPC++);
	op = *read_z80_memory(cpuPC++);
	addcycles(cddcbtable[op]);
	(*ddcbtable[op])();
}
void fdcb(){
  static void (*fdcbtable[0x100])() = {
 /*  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  a  |  b  |  c  |  d  |  e  |  f  |      */
	noop, noop, noop, noop, noop, noop,rlciy, noop, noop, noop, noop, noop, noop, noop,rrciy, noop, /* 0 */
	noop, noop, noop, noop, noop, noop, rliy, noop, noop, noop, noop, noop, noop, noop, rriy, noop, /* 1 */
	noop, noop, noop, noop, noop, noop,slaiy, noop, noop, noop, noop, noop, noop, noop,sraiy, noop, /* 2 */
	noop, noop, noop, noop, noop, noop,slliy, noop, noop, noop, noop, noop, noop, noop,srliy, noop, /* 3 */
	noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 4 */
	noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 5 */
	noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 6 */
	noop, noop, noop, noop, noop, noop,bitiy, noop, noop, noop, noop, noop, noop, noop,bitiy, noop, /* 7 */
	noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* 8 */
	noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* 9 */
	noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* a */
	noop, noop, noop, noop, noop, noop,resiy, noop, noop, noop, noop, noop, noop, noop,resiy, noop, /* b */
	noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* c */
	noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* d */
	noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* e */
	noop, noop, noop, noop, noop, noop,setiy, noop, noop, noop, noop, noop, noop, noop,setiy, noop, /* f */
	};
	displace = *read_z80_memory(cpuPC++);
	op = *read_z80_memory(cpuPC++);
	addcycles(cfdcbtable[op]);
	(*fdcbtable[op])();
}

/***********/
/* OPCODES */
/***********/

/* 8-BIT LOAD GROUP */

void ld()	{ /* LD r,r' */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *r[op & 7];
}
void ldim()	{ /* LD r,n */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[(op>>3) & 7] = *read_z80_memory(cpuPC++);
}
void ldhlr(){ /* LD (HL),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	write_z80_memory(*cpuHLreg, *r[op & 7]);
}
void lhlim(){ /* LD (HL),n */
	write_z80_memory(*cpuHLreg, *read_z80_memory(cpuPC++));
}
void ldixi(){ /* LD r,(IX+d) */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++));
}
void ldiyi(){ /* LD r,(IY+d) */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[(op >> 3) & 7] = *read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++));
}
void ldiix(){ /* LD (IX+d),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	write_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++), *r[op & 7]);
}
void ldiiy(){ /* LD (IY+d),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	write_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++), *r[op & 7]);
}
void ldixn(){ /* LD (IX+d),n */
	int8_t offset = *read_z80_memory(cpuPC++);
	uint8_t val = *read_z80_memory(cpuPC++);
	write_z80_memory(*cpuIXreg + offset, val);
}
void ldiyn(){ /* LD (IY+d),n */
	int8_t offset = *read_z80_memory(cpuPC++);
	uint8_t val = *read_z80_memory(cpuPC++);
	write_z80_memory(*cpuIYreg + offset, val);
}
void ldabc(){ /* LD A,(BC) */
	*cpuAreg = *read_z80_memory(*cpuBCreg);
}
void ldade(){ /* LD A,(DE) */
	*cpuAreg = *read_z80_memory(*cpuDEreg);
}
void ldai(){ /* LD A,(nn) */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	*cpuAreg = *read_z80_memory(address);
}
void ldbca(){ /* LD (BC),A */
	write_z80_memory(*cpuBCreg, *cpuAreg);
}
void lddea(){ /* LD (DE),A */
	write_z80_memory(*cpuDEreg, *cpuAreg);
}
void ldia()	{ /* LD (nn),A */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	write_z80_memory(address, *cpuAreg);
}
void ldain(){ /* LD A,I */
	*cpuAreg = cpuI;
}
void ldar()	{ /* LD A,R */
	*cpuAreg = cpuR;
}
void ldina(){ /* LD I,A */
	cpuI = *cpuAreg;
}
void ldra()	{ /* LD R,A */
	 cpuR = *cpuAreg;
}


/* 16-BIT LOAD GROUP */

void ld16()	{ /* LD dd,nn */
	*rp[((op>>4) & 3)] = (*read_z80_memory(cpuPC) | ((*read_z80_memory(cpuPC + 1)) << 8));
	cpuPC += 2;
}
void ldix()	{ /* LD IX,nn */
	*cpuIXreg = (*read_z80_memory(cpuPC) | ((*read_z80_memory(cpuPC + 1)) << 8));
	cpuPC += 2;
}
void ldiy()	{ /* LD IY,nn */
	*cpuIYreg = (*read_z80_memory(cpuPC) | ((*read_z80_memory(cpuPC + 1)) << 8));
	cpuPC += 2;
}
void ldhli(){ /* LD HL,(nn) */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	*cpuLreg = *read_z80_memory(address++);
	*cpuHreg = *read_z80_memory(address);
}
void ldrp()	{ /* LD dd,(nn) */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	*rp[(op >> 4) & 0x03] = *read_z80_memory(address++);
	*rp[(op >> 4) & 0x03] |= (*read_z80_memory(address) << 8);
}
void ldxin(){ /* LD IX,(nn) */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	*cpuIXlreg = *read_z80_memory(address++);
	*cpuIXhreg = *read_z80_memory(address);
}
void ldyin(){ /* LD IY,(nn) */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	*cpuIYlreg = *read_z80_memory(address++);
	*cpuIYhreg = *read_z80_memory(address);
}
void ldihl(){ /* LD (nn),HL */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	write_z80_memory(address++, *cpuLreg);
	write_z80_memory(address, *cpuHreg);
}
void ldidd(){ /* LD (nn),dd */
	uint16_t address;
	address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	write_z80_memory(address++, (*rp[(op >> 4) & 0x03] & 0xff));
	write_z80_memory(address, (*rp[(op >> 4) & 0x03] >> 8));
}
void linix(){ /* LD (nn),IX */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= (*read_z80_memory(cpuPC++) << 8);
	write_z80_memory(address++, *cpuIXlreg);
	write_z80_memory(address, *cpuIXhreg);
}
void liniy(){ /* LD (nn),IY */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= (*read_z80_memory(cpuPC++) << 8);
	write_z80_memory(address++, *cpuIYlreg);
	write_z80_memory(address, *cpuIYhreg);
}
void lsphl(){ /* LD SP,HL */
	cpuSP = *cpuHLreg;
}
void lspix(){ /* LD SP,IX */
	cpuSP = *cpuIXreg;
}
void lspiy(){ /* LD SP,IY */
	cpuSP = *cpuIYreg;
}
void push()	{ /* PUSH qq */
	write_z80_memory(--cpuSP, ((*rp2[(op>>4) & 3] & 0xff00) >> 8));
	write_z80_memory(--cpuSP, ( *rp2[(op>>4) & 3] & 0x00ff));
}
void pusix(){ /* PUSH IX */
	write_z80_memory(--cpuSP, *cpuIXhreg);
	write_z80_memory(--cpuSP, *cpuIXlreg);
}
void pusiy(){ /* PUSH IY */
	write_z80_memory(--cpuSP, *cpuIYhreg);
	write_z80_memory(--cpuSP, *cpuIYlreg);
}
void pop()	{ /* POP qq */
	*rp2[(op>>4) & 3] = (*read_z80_memory(cpuSP) | ((*read_z80_memory(cpuSP + 1)) << 8));
	cpuSP += 2;
}
void popix(){ /* POP IX */
	*cpuIXreg = (*read_z80_memory(cpuSP) | ((*read_z80_memory(cpuSP + 1)) << 8));
	cpuSP += 2;
}
void popiy(){ /* POP IY */
	*cpuIYreg = (*read_z80_memory(cpuSP) | ((*read_z80_memory(cpuSP + 1)) << 8));
	cpuSP += 2;
}


/* EXCHANGE, BLOCK TRANSFER, SEARCH GROUP */

void ex()	{ /* EX DE,HL */
	uint16_t tmp = cpuDE;
	cpuDE = cpuHL;
	cpuHL = tmp;
}
void exaf()	{ /* EX AF,AF' */
	uint16_t tmp = cpuAF;
	cpuAF = cpuAFx;
	cpuAFx = tmp;
}
void exx()	{ /* EXX */
	uint16_t tmpBC = cpuBC, tmpDE = cpuDE, tmpHL = cpuHL;
	cpuBC = cpuBCx;
	cpuDE = cpuDEx;
	cpuHL = cpuHLx;
	cpuBCx = tmpBC;
	cpuDEx = tmpDE;
	cpuHLx = tmpHL;
}
void exhl()	{ /* EX (SP),HL */
	uint16_t tmp = (*read_z80_memory(cpuSP) | (*read_z80_memory(cpuSP+1) << 8));
	write_z80_memory(cpuSP, *cpuLreg);
	write_z80_memory(cpuSP+1, *cpuHreg);
	cpuHL = tmp;
}
void exix()	{ /* EX (SP),IX */
	uint16_t tmp = (*read_z80_memory(cpuSP) | (*read_z80_memory(cpuSP+1) << 8));
	write_z80_memory(cpuSP, *cpuIXlreg);
	write_z80_memory(cpuSP+1, *cpuIXhreg);
	cpuIX = tmp;
}
void exiy()	{ /* EX (SP),IY */
	uint16_t tmp = (*read_z80_memory(cpuSP) | (*read_z80_memory(cpuSP+1) << 8));
	write_z80_memory(cpuSP, *cpuIYlreg);
	write_z80_memory(cpuSP+1, *cpuIYhreg);
	cpuIY = tmp;
}
void ldi()	{ /* LDI */
	(*cpuBCreg)--;
	write_z80_memory((*cpuDEreg)++, *read_z80_memory((*cpuHLreg)++));
	*cpuFreg = ((*cpuFreg & SZYXC_FLAG) | (*cpuBCreg ? P_FLAG : 0));
}
void ldir()	{ /* LDIR */
	(*cpuBCreg)--;
	write_z80_memory((*cpuDEreg)++, *read_z80_memory((*cpuHLreg)++));
	if (*cpuBCreg){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = ((*cpuFreg & SZYXC_FLAG) | (*cpuBCreg ? P_FLAG : 0));
}
void ldd()	{ /* LDD */
	write_z80_memory((*cpuDEreg)--, *read_z80_memory((*cpuHLreg)--));
	(*cpuBCreg)--;
	*cpuFreg = ((*cpuFreg & SZYXC_FLAG) | (*cpuBCreg ? P_FLAG : 0));
}
void lddr()	{ /* LDDR */
	(*cpuBCreg)--;
	write_z80_memory((*cpuDEreg)--, *read_z80_memory((*cpuHLreg)--));
	if (*cpuBCreg){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = ((*cpuFreg & SZYXC_FLAG) | (*cpuBCreg ? P_FLAG : 0));
}
void cpi()	{ /* CPI */
	uint8_t val = *read_z80_memory((*cpuHLreg)++);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (res & S_FLAG) | (!(res) << Z_SHIFT) | (((*cpuAreg & 0xf) < (val & 0xf)) << H_SHIFT) | (*cpuBCreg ? P_FLAG : 0) | N_FLAG);
}
void cpir()	{ /* CPIR */
	uint8_t val = *read_z80_memory((*cpuHLreg)++);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	if (*cpuBCreg && res){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (res & S_FLAG) | (!(res) << Z_SHIFT) | (((*cpuAreg & 0xf) < (val & 0xf)) << H_SHIFT) | (*cpuBCreg ? P_FLAG : 0) | N_FLAG);
}
void cpd()	{ /* CPD */
	uint8_t val = *read_z80_memory((*cpuHLreg)--);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (res & S_FLAG) | (!(res) << Z_SHIFT) | (((*cpuAreg & 0xf) < (val & 0xf)) << H_SHIFT) | (*cpuBCreg ? P_FLAG : 0) | N_FLAG);
}
void cpdr()	{ /* CPDR */
	uint8_t val = *read_z80_memory((*cpuHLreg)--);
	(*cpuBCreg)--;
	uint16_t res = *cpuAreg - val;
	if (*cpuBCreg && res){
		cpuPC -= 2;
		addcycles(5);
	}
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (res & S_FLAG) | (!(res) << Z_SHIFT) | (((*cpuAreg & 0xf) < (val & 0xf)) << H_SHIFT) | (*cpuBCreg ? P_FLAG : 0) | N_FLAG);
}


/* 8-BIT ARITHMETIC GROUP */

void add(uint8_t value)	{
	uint16_t res = *cpuAreg + value;
	*cpuFreg = ((res & S_FLAG)     | (!(res & 0xff) << Z_SHIFT) 			  		|
			   (*cpuFreg & YX_FLAG) |	((*cpuAreg ^ value ^ res) & H_FLAG) 		|
			   					   (((*cpuAreg ^ res) & (value ^ res) & 0x80) >> 5) |
			   	   	   	   	   	   ((res & 0x100) >> 8));
	*cpuAreg = res;
}
void addr()	{ /* ADD A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	add(*r[op & 7]);
}
void addi()	{ /* ADD A,n */
	add(*read_z80_memory(cpuPC++));
}
void adixi(){ /* ADD A,(IX+d) */
	add(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void adiyi(){ /* ADD A,(IY+d) */
	add(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void adc(uint8_t value)	{
	uint16_t res = *cpuAreg + value + (*cpuFreg & 0x01);
	*cpuFreg = ((res & S_FLAG)     | (!(res & 0xff) << Z_SHIFT) 					|
			   (*cpuFreg & YX_FLAG) | ((*cpuAreg ^ value ^ res) & H_FLAG)			|
			   					   (((*cpuAreg ^ res) & (value ^ res) & 0x80) >> 5) |
			   	   	   	   	   	   ((res & 0x100) >> 8));
	*cpuAreg = res;
}
void adcr()	{ /* ADC A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	adc(*r[op & 7]);
}
void adci()	{ /* ADC A,n */
	adc(*read_z80_memory(cpuPC++));
}
void acixi(){ /* ADC A,(IX+d) */
	adc(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void aciyi(){ /* ADC A,(IY+d) */
	adc(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void sub(uint8_t value)	{
	uint16_t res = *cpuAreg - value;
	*cpuFreg = ((res & S_FLAG)     | (!(res & 0xff) << Z_SHIFT) 			  			 |
			   (*cpuFreg & YX_FLAG) | ((*cpuAreg ^ value ^ res) & H_FLAG) 				 |
			   					   (((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) >> 5) |
			   N_FLAG			 | ((res & 0x100) >> 8));
	*cpuAreg = res;
}
void subr()	{ /* SUB A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	sub(*r[op & 7]);
}
void subi()	{ /* SUB A,n */
	sub(*read_z80_memory(cpuPC++));
}
void sbixi(){ /* SUB A,(IX+d) */
	sub(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void sbiyi(){ /* SUB A,(IY+d) */
	sub(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void sbc(uint8_t value)	{
	uint16_t res = *cpuAreg - value - (*cpuFreg & C_FLAG);
	*cpuFreg = ((res & S_FLAG) | (!(res & 0xff) << Z_SHIFT) | (*cpuFreg & YX_FLAG) | ((*cpuAreg ^ value ^ res) & H_FLAG) | (((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) >> 5) | N_FLAG | ((res & 0x100) >> 8));
	*cpuAreg = res;
}
void sbcr()	{ /* SBC A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	sbc(*r[op & 7]);
}
void sbci()	{ /* SBC A,n */
	sbc(*read_z80_memory(cpuPC++));
}
void scixi(){ /* SBC A,(IX+d) */
	sbc(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void sciyi(){ /* SBC A,(IY+d) */
	sbc(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void and(uint8_t value)	{
	*cpuAreg &= value;
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuAreg & S_FLAG) | ((!*cpuAreg) << Z_SHIFT) | H_FLAG | parcalc(*cpuAreg));
}
void andr()	{ /* AND A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	and(*r[op & 7]);
}
void andi()	{ /* AND A,n */
	and(*read_z80_memory(cpuPC++));
}
void anixi(){ /* AND A,(IX+d) */
	and(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void aniyi(){ /* AND A,(IY+d) */
	and(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void or(uint8_t value){
	*cpuAreg |= value;
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuAreg & S_FLAG) | ((!*cpuAreg) << Z_SHIFT) | parcalc(*cpuAreg));
}
void orr()	{ /* OR A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	or(*r[op & 7]);
}
void ori()	{ /* OR A,n */
	or(*read_z80_memory(cpuPC++));
}
void orixi(){ /* OR A,(IX+d) */
	int8_t tmp = *read_z80_memory(cpuPC++);
	or(*read_z80_memory(*cpuIXreg + tmp));
}
void oriyi(){ /* OR A,(IY+d) */
	int8_t tmp = *read_z80_memory(cpuPC++);
	or(*read_z80_memory(*cpuIYreg + tmp));
}
void xor(uint8_t value)	{
	*cpuAreg ^= value;
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuAreg & S_FLAG) | ((!*cpuAreg) << Z_SHIFT) | parcalc(*cpuAreg));
}
void xorr()	{ /* XOR A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	xor(*r[op & 7]);
}
void xori()	{ /* XOR A,n */
	xor(*read_z80_memory(cpuPC++));
}
void xrixi(){ /* XOR A,(IX+d) */
	int8_t tmp = *read_z80_memory(cpuPC++);
	xor(*read_z80_memory(*cpuIXreg + tmp));
}
void xriyi(){ /* XOR A,(IY+d) */
	int8_t tmp = *read_z80_memory(cpuPC++);
	xor(*read_z80_memory(*cpuIYreg + tmp));
}
void cp(uint8_t value)	{
	uint16_t res = *cpuAreg - value;
	*cpuFreg = ((res & S_FLAG) | ((!(res & 0xff)) << Z_SHIFT) |
			   ((*cpuAreg ^ value ^ res) & H_FLAG) | (((*cpuAreg ^ value) & (*cpuAreg ^ res) & 0x80) >> 5) |
			   N_FLAG | ((res & 0x100) >> 8));
}
void cpr()	{ /* CP A,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	cp(*r[op & 7]);
}
void cpn()	{ /* CP A,n */
	cp(*read_z80_memory(cpuPC++));
}
void cpixi(){ /* CP A,(IX+d) */
	cp(*read_z80_memory(*cpuIXreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void cpiyi(){ /* CP A,(IY+d) */
	cp(*read_z80_memory(*cpuIYreg + (int8_t)*read_z80_memory(cpuPC++)));
}
void inc()	{ /* INC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])++;
	*cpuFreg = ((*r[(op>>3) & 7] & S_FLAG) | (!*r[(op>>3) & 7] << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_SHIFT) | ((tmp == 0x7f) << P_SHIFT) | (*cpuFreg & C_FLAG));
}
void inchl(){ /* INC (HL) */
	uint8_t tmp = *read_z80_memory(*cpuHLreg);
	uint8_t val = tmp + 1;
	write_z80_memory(*cpuHLreg, val);
	*cpuFreg = ((val & S_FLAG) | (!val << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_SHIFT) | ((tmp == 0x7f) << P_SHIFT) | (*cpuFreg & C_FLAG));
}
void inixi(){ /* INC (IX+d) */
	int8_t val = *read_z80_memory(cpuPC++);
	uint8_t tmp = *read_z80_memory(*cpuIXreg + val);
	write_z80_memory(*cpuIXreg + val, tmp + 1);
	*cpuFreg = (((tmp + 1) & S_FLAG) | ((tmp == 0xff) << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_SHIFT) | ((tmp == 0x7f) << P_SHIFT) | (*cpuFreg & C_FLAG));
}
void iniyi(){ /* INC (IY+d) */
	int8_t val = *read_z80_memory(cpuPC++);
	uint8_t tmp = *read_z80_memory(*cpuIYreg + val);
	write_z80_memory(*cpuIYreg + val, tmp + 1);
	*cpuFreg = (((tmp + 1) & S_FLAG) | ((tmp == 0xff) << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_SHIFT) | ((tmp == 0x7f) << P_SHIFT) | (*cpuFreg & C_FLAG));
}
void dec()	{ /* DEC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg}, tmp;
	tmp = (*r[(op>>3) & 7]);
	(*r[(op>>3) & 7])--;
	*cpuFreg = ((*r[(op>>3) & 7] & S_FLAG) | (!*r[(op>>3) & 7] << Z_SHIFT) | (!(tmp & 0x0f) << H_SHIFT) | ((tmp == 0x80) << P_SHIFT) | N_FLAG | (*cpuFreg & C_FLAG));
}
void dechl(){ /* DEC (HL) */
	uint8_t tmp = *read_z80_memory(*cpuHLreg);
	uint8_t val = tmp - 1;
	write_z80_memory(*cpuHLreg, val);
	*cpuFreg = ((val & S_FLAG) | (!val << Z_SHIFT) | (!(tmp & 0x0f) << H_SHIFT) | ((tmp == 0x80) << P_SHIFT) | N_FLAG | (*cpuFreg & C_FLAG));
}
void deixi(){ /* DEC (IX+d) */
	int8_t val = *read_z80_memory(cpuPC++);
	uint8_t tmp = *read_z80_memory(*cpuIXreg + val);
	write_z80_memory(*cpuIXreg + val, tmp - 1);
	*cpuFreg = (((tmp - 1) & S_FLAG) | ((tmp == 0x01) << Z_SHIFT) | (!(tmp & 0x0f) << H_SHIFT) | ((tmp == 0x80) << P_SHIFT) | N_FLAG | (*cpuFreg & C_FLAG));
}
void deiyi(){ /* DEC (IY+d) */
	int8_t val = *read_z80_memory(cpuPC++);
	uint8_t tmp = *read_z80_memory(*cpuIYreg + val);
	write_z80_memory(*cpuIYreg + val, tmp - 1);
	*cpuFreg = (((tmp - 1) & S_FLAG) | ((tmp == 0x01) << Z_SHIFT) | (!(tmp & 0x0f) << H_SHIFT) | ((tmp == 0x80) << P_SHIFT) | N_FLAG | (*cpuFreg & C_FLAG));
}


/* GENERAL PURPOSE OPCODES */

void daa()	{ /* DAA */
	uint8_t tmp = *cpuAreg;
	if((*cpuFreg & H_FLAG) || ((*cpuAreg & 0x0f) > 9))
		tmp += (*cpuFreg & N_FLAG) ? -0x06 : 0x06;
	if((*cpuFreg & C_FLAG) || (*cpuAreg > 0x99))
		tmp += (*cpuFreg & N_FLAG) ? -0x60 : 0x60;
	*cpuFreg = ((tmp & S_FLAG) | (!tmp << Z_SHIFT) | ((tmp ^ *cpuAreg) & H_FLAG) | (parcalc(tmp)) | (*cpuFreg & N_FLAG) | ((*cpuFreg & C_FLAG) | (*cpuAreg > 0x99)));
	*cpuAreg = tmp;
}
void cpl()	{ /* CPL */
	*cpuAreg ^= 0xff;
	*cpuFreg = ((*cpuFreg & 0xed) | H_FLAG | N_FLAG);
}
void neg()	{ /* NEG */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = (0 - *cpuAreg);
	*cpuFreg = ((*cpuAreg & S_FLAG) | ((!*cpuAreg) << Z_SHIFT) | ((tmp & 0xf) ? H_FLAG : 0) | ((tmp == 0x80) << P_SHIFT) | N_FLAG | (tmp ? C_FLAG : 0));
}
/* TODO */
void ccf()	{ /* CCF */
	*cpuFreg = (((*cpuFreg & C_FLAG) << H_FLAG) | ((*cpuFreg & SZYXPC_FLAG) ^ C_FLAG));
}
void scf()	{ /* SCF */
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | C_FLAG);
}
void nop()	{ /* NOP */
}
void halt()	{ /* HALT */
	halted = 1;
	cpuPC--;
}
void di() 	{ /* DI */
	iff1 = iff2 = 0;
	intDelay = 1;
}
void ei() 	{ /* EI */
	iff1 = iff2 = 1;
	intDelay = 1;
}
void im()	{ /* IM */
	uint8_t im[8] = {0, 0, 1, 2, 0, 0, 1, 2};
	iMode = im[(op>>3) & 7];
	if (iMode != 1){
		printf("Unsupported interrupt mode: %i\n",iMode);
	}
}


/* 16-BIT ARITHMETIC GROUP */

void addrp(){ /* ADD HL,ss */
	uint32_t res = *cpuHLreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | ((((*cpuHLreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff)) > 0xfff) << H_FLAG) | (res > 0xffff));
	*cpuHLreg = res;
}
void adcrp(){ /* ADC HL,ss */
	uint32_t res = *cpuHLreg + *rp[(op >> 4) & 0x03] + (*cpuFreg & C_FLAG);
	*cpuFreg = (((res & 0x8000) >> 8) | (!(res & 0xffff) << Z_SHIFT) | (((*cpuHLreg ^ *rp[(op >> 4) & 0x03] ^ res) & 0x1000) >> 8) |
			(((*cpuHLreg ^ res) & (*rp[(op >> 4) & 0x03] ^ res) & 0x8000) >> 13) | ((res & 0x10000) >> 16));
	*cpuHLreg = res;
}
void sbcrp(){ /* SBC HL,ss */
	uint32_t res = *cpuHLreg - *rp[(op >> 4) & 0x03] - (*cpuFreg & C_FLAG);
	*cpuFreg = (((res & 0x8000) >> 8) | (!(res & 0xffff) << Z_SHIFT) | (((*cpuHLreg ^ *rp[(op >> 4) & 0x03] ^ res) & 0x1000) >> 8) |
			(((*cpuHLreg ^ *rp[(op >> 4) & 0x03]) & (*cpuHLreg ^ res) & 0x8000) >> 13) | N_FLAG | ((res & 0x10000) >> 16));
	*cpuHLreg = res;
}
void addix(){ /* ADD IX,pp */
	uint32_t res = *cpuIXreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | ((((*cpuIXreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff)) > 0xfff) << H_FLAG) | (res > 0xffff));
	*cpuIXreg = res;
}
void addiy(){ /* ADD IY,rr */
	uint32_t res = *cpuIYreg + *rp[(op >> 4) & 0x03];
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | ((((*cpuIYreg & 0xfff) + (*rp[(op >> 4) & 0x03] & 0xfff)) > 0xfff) << H_FLAG) | (res > 0xffff));
	*cpuIYreg = res;
}
void incrp(){ /* INC ss */
	(*rp[(op>>4) & 3])++;
}
void incix(){ /* INC IX */
	(*cpuIXreg)++;
}
void inciy(){ /* INC IY */
	(*cpuIYreg)++;
}
void decrp(){ /* DEC ss */
	(*rp[(op>>4) & 3])--;
}
void decix(){ /* DEC IX */
	(*cpuIXreg)--;
}
void deciy(){ /* DEC IY */
	(*cpuIYreg)--;
}


/* ROTATE AND SHIFT GROUP */

void rlca()	{ /* RLCA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (tmp >> 7));
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | (tmp >> 7));
}
void rla()	{ /* RLA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg << 1) | (*cpuFreg & 0x01));
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | (tmp >> 7));
}
void rrca()	{ /* RRCA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = (*cpuAreg >> 1);
	*cpuAreg |= (tmp << 7);
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | (tmp & C_FLAG));
}
void rra()	{ /* RRA */
	uint8_t tmp = *cpuAreg;
	*cpuAreg = ((*cpuAreg >> 1) | (*cpuFreg << 7));
	*cpuFreg = ((*cpuFreg & SZYXP_FLAG) | (tmp & C_FLAG));
}
void rlc()	{ /* RLC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | (tmp >> 7));
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | ((tmp & 0x80) >> 7));
}
void rlcix(){ /* RLC (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (tmp >> 7));
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void rlciy(){ /* RLC (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (tmp >> 7));
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void rl()	{ /* RL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | (*cpuFreg & 0x01));
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | ((tmp & 0x80) >> 7));
}
void rlix()	{ /* RL (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (*cpuFreg & C_FLAG));
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void rliy()	{ /* RL (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | (*cpuFreg & C_FLAG));
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void rrc()	{ /* RRC r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (tmp << 7));
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | (tmp & C_FLAG));
}
void rrcix(){ /* RRC (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp << 7));
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void rrciy(){ /* RRC (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp << 7));
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void rr()	{ /* RR r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (*cpuFreg << 7));
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | (tmp & C_FLAG));
}
void rrix()	{ /* RR (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (*cpuFreg << 7));
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void rriy()	{ /* RR (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (*cpuFreg << 7));
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void sla()	{ /* SLA r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = (*r[op & 7] << 1);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | ((tmp & 0x80) >> 7));
}
void slaix(){ /* SLA (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = (val << 1);
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void slaiy(){ /* SLA (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = (val << 1);
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void sra()	{ /* SRA r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] >> 1) | (tmp & 0x80));
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | (tmp & C_FLAG));
}
void sraix(){ /* SRA (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp & 0x80));
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void sraiy(){ /* SRA (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val >> 1) | (tmp & 0x80));
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void sll()	{ /* SLL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*r[op & 7] = ((*r[op & 7] << 1) | 0x01);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*r[op & 7] & S_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | ((tmp & 0x80) >> 7));
}
void sllix(){ /* SLL (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | 0x01);
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void slliy(){ /* SLL (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = ((val << 1) | 0x01);
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (val & S_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | ((tmp & 0x80) >> 7));
}
void srl()	{ /* SRL r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t tmp = *r[op & 7];
	*cpuFreg = (*r[op & 7] & 0x01) ? (*cpuFreg | 0x01) : (*cpuFreg & ~0x01); /* C flag */
	*r[op & 7] = (*r[op & 7] >> 1);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | ((!*r[op & 7]) << Z_SHIFT) | parcalc(*r[op & 7]) | (tmp & C_FLAG));
}
void srlix(){ /* SRL (IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	uint8_t tmp = val;
	val = (val >> 1);
	write_z80_memory(*cpuIXreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void srliy(){ /* SRL (IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	uint8_t tmp = val;
	val = (val >> 1);
	write_z80_memory(*cpuIYreg + displace, val);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | ((!val) << Z_SHIFT) | parcalc(val) | (tmp & C_FLAG));
}
void rld()	{ /* RLD */
	uint8_t tmp = *cpuAreg;
	uint8_t val = *read_z80_memory(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | ((val & 0xf0) >> 4));
	val = (val << 4);
	val |= (tmp & 0x0f);
	write_z80_memory(*cpuHLreg, val);
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuAreg & S_FLAG)| ((!*cpuAreg) << Z_SHIFT) | parcalc(*cpuAreg));
}
void rrd()	{ /* RRD */
	uint8_t tmp = *cpuAreg;
	uint8_t val = *read_z80_memory(*cpuHLreg);
	*cpuAreg = ((*cpuAreg & 0xf0) | (val & 0x0f));
	val = (val >> 4);
	val |= (tmp << 4);
	write_z80_memory(*cpuHLreg, val);
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuAreg & S_FLAG)| ((!*cpuAreg) << Z_SHIFT) | parcalc(*cpuAreg));
}


/* BIT SET, RESET, TEST GROUP */

void bit()	{ /* BIT b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	uint8_t val = *r[op & 7] & (1 << ((op >> 3) & 7));
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | ((val && ((op >> 3) & 7) == 7) << S_SHIFT) | ((!val) << Z_SHIFT) | H_FLAG | ((!val) << P_SHIFT));
}
void bitix(){ /* BIT b,(IX+d) */
	uint8_t val = *read_z80_memory(*cpuIXreg + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | ((val && ((op >> 3) & 7) == 7) << S_SHIFT) | ((!val) << Z_SHIFT) | H_FLAG | ((!val) << P_SHIFT));
}
void bitiy(){ /* BIT b,(IY+d) */
	uint8_t val = *read_z80_memory(*cpuIYreg + displace);
	val = val & (1 << ((op >> 3) & 7));
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | ((val && ((op >> 3) & 7) == 7) << S_SHIFT) | ((!val) << Z_SHIFT) | H_FLAG | ((!val) << P_SHIFT));
}
void set()	{ /* SET b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[op & 7] |= (1 << ((op >> 3) & 7));
}
void setix(){ /* SET b,(IX+d) */
	uint8_t tmp = *read_z80_memory(*cpuIXreg + displace) | (1 << ((op >> 3) & 7));
	write_z80_memory(*cpuIXreg + displace, tmp);
}
void setiy(){ /* SET b,(IY+d) */
	uint8_t tmp = *read_z80_memory(*cpuIYreg + displace) | (1 << ((op >> 3) & 7));
	write_z80_memory(*cpuIYreg + displace, tmp);
}
void res()	{ /* RES b,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	*r[op & 7] &= ~(1 << ((op >> 3) & 7));
}
void resix(){ /* RES b,(IX+d) */
	uint8_t tmp = *read_z80_memory(*cpuIXreg + displace) & ~(1 << ((op >> 3) & 7));
	write_z80_memory(*cpuIXreg + displace, tmp);
}
void resiy(){ /* RES b,(IY+d) */
	uint8_t tmp = *read_z80_memory(*cpuIYreg + displace) & ~(1 << ((op >> 3) & 7));
	write_z80_memory(*cpuIYreg + displace, tmp);
}


/* JUMP GROUP */

void jp()	{ /* JP nn */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	cpuPC = address;
}
void jpc()	{ /* JP cc,nn */
	uint8_t cc[8] = {!(*cpuFreg & Z_FLAG), (*cpuFreg & Z_FLAG), !(*cpuFreg & C_FLAG), (*cpuFreg & C_FLAG), !(*cpuFreg & P_FLAG), (*cpuFreg & P_FLAG), !(*cpuFreg & S_FLAG), (*cpuFreg & S_FLAG)};
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= ((*read_z80_memory(cpuPC++)) << 8);
	if(cc[(op>>3) & 7])
		cpuPC = address;
}
void jr()	{ /* JR e */
	cpuPC += ((int8_t)*read_z80_memory(cpuPC) + 1);
}
void jrc()	{ /* JR cc,e */
	uint8_t cc[4] = {!(*cpuFreg & Z_FLAG), (*cpuFreg & Z_FLAG), !(*cpuFreg & C_FLAG), (*cpuFreg & C_FLAG)};
	if(cc[((op>>3) & 7) - 4]) {
		jr();
		addcycles(5);
	}
	else
		cpuPC++;
}
void jphl()	{ /* JP (HL) */
	cpuPC = *cpuHLreg;
}
void jpix()	{ /* JP (IX) */
	cpuPC = *cpuIXreg;
}
void jpiy()	{ /* JP (IY) */
	cpuPC = *cpuIYreg;
}
void djnz()	{ /* DJNZ,e */
	(*cpuBreg)--;
	if(*cpuBreg) {
		cpuPC += ((int8_t)*read_z80_memory(cpuPC) + 1);
		addcycles(5);
	}
	else
		cpuPC++;
}


/* CALL AND RETURN GROUP */

void call()	{ /* CALL nn */
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= (*read_z80_memory(cpuPC++) << 8);
	write_z80_memory(--cpuSP, ((cpuPC & 0xff00) >> 8));
	write_z80_memory(--cpuSP, (cpuPC & 0x00ff));
	cpuPC = address;
}
void callc(){ /* CALL cc,nn */
	uint8_t cc[8] = {!(*cpuFreg & Z_FLAG), (*cpuFreg & Z_FLAG), !(*cpuFreg & C_FLAG), (*cpuFreg & C_FLAG), !(*cpuFreg & P_FLAG), (*cpuFreg & P_FLAG), !(*cpuFreg & S_FLAG), (*cpuFreg & S_FLAG)};
	uint16_t address = *read_z80_memory(cpuPC++);
	address |= (*read_z80_memory(cpuPC++) << 8);
	if(cc[(op>>3) & 7]) {
		write_z80_memory(--cpuSP, ((cpuPC & 0xff00) >> 8));
		write_z80_memory(--cpuSP, (cpuPC & 0x00ff));
		cpuPC = address;
		addcycles(7);
	}
}
void ret()	{ /* RET */
	uint16_t address = *read_z80_memory(cpuSP++);
	address |= ((*read_z80_memory(cpuSP++)) << 8);
	cpuPC = address;
}
void retc()	{ /* RET cc */
	uint8_t cc[8] = {!(*cpuFreg & Z_FLAG), (*cpuFreg & Z_FLAG), !(*cpuFreg & C_FLAG), (*cpuFreg & C_FLAG), !(*cpuFreg & P_FLAG), (*cpuFreg & P_FLAG), !(*cpuFreg & S_FLAG), (*cpuFreg & S_FLAG)};
	if (cc[(op >> 3) & 7]){
		ret();
		addcycles(6);
	}
}
void reti()	{ /* RETI */
	/* TODO: incomplete */
	uint16_t address = *read_z80_memory(cpuSP++);
	address |= ((*read_z80_memory(cpuSP++)) << 8);
	cpuPC = address;
}
void retn()	{ /* RETN */
	/* TODO: incomplete */
	uint16_t address = *read_z80_memory(cpuSP++);
	address |= ((*read_z80_memory(cpuSP++)) << 8);
	cpuPC = address;
	iff1 = iff2;
}
void rst()	{ /* RST */
	write_z80_memory(--cpuSP, ((cpuPC & 0xff00) >> 8));
	write_z80_memory(--cpuSP, ( cpuPC & 0x00ff));
	cpuPC = (op & 0x38);
}


/* INPUT AND OUTPUT GROUP */

void in()	{ /* IN A,(n) */
	synchronize(0);
	*cpuAreg = read_z80_register(*read_z80_memory(cpuPC++));
}
void inrc()	{ /* IN r,(C) */
	uint8_t none; /* TODO: neater solution */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, &none, cpuAreg};
	synchronize(0);
	uint8_t tmp = read_z80_register(*cpuCreg);
	*r[(op >> 3) & 7] = tmp;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (tmp & S_FLAG) | ((!tmp) << Z_SHIFT) | parcalc(tmp));
}
void ini()	{ /* INI */
	synchronize(0);
	uint8_t tmp = read_z80_register(*cpuCreg);
	write_z80_memory(*cpuHLreg, tmp);
	(*cpuBreg)--; /* byte counter */
	(*cpuHLreg)++;
	uint16_t k = (((*cpuCreg + 1) & 0xff) + tmp);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}
void inir()	{ /* INIR */
	synchronize(0);
	uint8_t tmp = read_z80_register(*cpuCreg);
	write_z80_memory(*cpuHLreg, tmp);
(*cpuBreg)--; /* byte counter */
(*cpuHLreg)++;
if (*cpuBreg){
	cpuPC -= 2;
	addcycles(5);
}
uint16_t k = (((*cpuCreg + 1) & 0xff) + tmp);
*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}
void out()	{ /* OUT (n),A */
	synchronize(0);
	write_z80_register(*read_z80_memory(cpuPC++), *cpuAreg);
}
void outc()	{ /* OUT (C),r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuHreg, cpuLreg, read_z80_memory(*cpuHLreg), cpuAreg};
	synchronize(1);
	write_z80_register(*cpuCreg, *r[(op >> 3) & 7]);
}
void outi()	{ /* OUTI */
	uint8_t tmp = *read_z80_memory(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	synchronize(1);
	write_z80_register(*cpuCreg, tmp);
	(*cpuHLreg)++;
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}
void otir()	{ /* OTIR */
uint8_t tmp = *read_z80_memory(*cpuHLreg); /* to be written to port */
(*cpuBreg)--; /* byte counter */
synchronize(1);
write_z80_register(*cpuCreg, tmp);
(*cpuHLreg)++;
if (*cpuBreg){
	cpuPC -= 2;
	addcycles(5);
}
uint16_t k = (*cpuLreg + tmp);
*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}
void outd()	{ /* OUTD */
	uint8_t tmp = *read_z80_memory(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	synchronize(1);
	write_z80_register(*cpuCreg, tmp);
	(*cpuHLreg)--;
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}
void otdr()	{ /* OUTD */
	uint8_t tmp = *read_z80_memory(*cpuHLreg); /* to be written to port */
	(*cpuBreg)--; /* byte counter */
	synchronize(1);
	write_z80_register(*cpuCreg, tmp);
	(*cpuHLreg)--;
	if (*cpuBreg){
		cpuPC -= 2;
		addcycles(5);
	}
	uint16_t k = (*cpuLreg + tmp);
	*cpuFreg = ((*cpuFreg & YX_FLAG) | (*cpuBreg & S_FLAG) | ((!*cpuBreg) << Z_SHIFT) | ((k > 0xff) << H_SHIFT) | parcalc((k & 7) ^ *cpuBreg) | ((tmp & 0x80) >> 6) | (k > 0xff));
}


/* UNDOCUMENTED OPCODES */

void ldixh(){ /* LD IXh,n */
	*cpuIXhreg = *read_z80_memory(cpuPC++);
}
void ldiyh(){ /* LD IYh,n */
	*cpuIYhreg = *read_z80_memory(cpuPC++);
}
void ldixl(){ /* LD IXl,n */
	*cpuIXlreg = *read_z80_memory(cpuPC++);
}
void ldiyl(){ /* LD IYl,n */
	*cpuIYlreg = *read_z80_memory(cpuPC++);
}
void lrixh(){ /* LD r,IXh */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIXhreg;
}
void lrixl(){ /* LD r,IXl */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIXlreg;
}
void lriyh(){ /* LD r,IYh */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIYhreg;
}
void lriyl(){ /* LD r,IYl */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*r[(op>>3) & 7] = *cpuIYlreg;
}
void lixhr(){ /* LD ixh,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*cpuIXhreg = *r[op & 0x07];
}
void lixlr(){ /* LD ixl,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIXhreg, cpuIXlreg, 0, cpuAreg};
	*cpuIXlreg = *r[op & 0x07];
}
void liyhr(){ /* LD iyh,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIYhreg, cpuIYlreg, 0, cpuAreg};
	*cpuIYhreg = *r[op & 0x07];
}
void liylr(){ /* LD iyl,r */
	uint8_t *r[8] = {cpuBreg, cpuCreg, cpuDreg, cpuEreg, cpuIYhreg, cpuIYlreg, 0, cpuAreg};
	*cpuIYlreg = *r[op & 0x07];
}
void adixh(){ /* ADD A,IXh */
	add(*cpuIXhreg);
}
void adixl(){ /* ADD A,IXl */
	add(*cpuIXlreg);
}
void adiyh(){ /* ADD A,IYh */
	add(*cpuIYhreg);
}
void adiyl(){ /* ADD A,IYl */
	add(*cpuIYlreg);
}
void acixh(){ /* ADD A,IXh */
	adc(*cpuIXhreg);
}
void acixl(){ /* ADC A,IXl */
	adc(*cpuIXlreg);
}
void aciyh(){ /* ADC A,IYh */
	adc(*cpuIYhreg);
}
void aciyl(){ /* ADC A,IYl */
	adc(*cpuIYlreg);
}
void sbixh(){ /* SUB A,IXh */
	sub(*cpuIXhreg);
}
void sbixl(){ /* SUB A,IXl */
	sub(*cpuIXlreg);
}
void sbiyh(){ /* SUB A,IYh */
	sub(*cpuIYhreg);
}
void sbiyl(){ /* SUB A,IYl */
	sub(*cpuIYlreg);
}
void scixh(){ /* SBC A,IXh */
	sbc(*cpuIXhreg);
}
void scixl(){ /* SBC A,IXl */
	sbc(*cpuIXlreg);
}
void sciyh(){ /* SBC A,IYh */
	sbc(*cpuIYhreg);
}
void sciyl(){ /* SBC A,IYl */
	sbc(*cpuIYlreg);
}
void anixh(){ /* AND A,IXh */
	and(*cpuIXhreg);
}
void anixl(){ /* AND A,IXl */
	and(*cpuIXlreg);
}
void aniyh(){ /* AND A,IYh */
	and(*cpuIYhreg);
}
void aniyl(){ /* AND A,IYl */
	and(*cpuIYlreg);
}
void orixh(){ /* OR A,IXh */
	or(*cpuIXhreg);
}
void orixl(){ /* OR A,IXl */
	or(*cpuIXlreg);
}
void oriyh(){ /* OR A,IYh */
	or(*cpuIYhreg);
}
void oriyl(){ /* OR A,IYl */
	or(*cpuIYlreg);
}
void xrixh(){ /* XOR A,IXh */
	xor(*cpuIXhreg);
}
void xrixl(){ /* XOR A,IXl */
	xor(*cpuIXlreg);
}
void xriyh(){ /* XOR A,IYh */
	xor(*cpuIYhreg);
}
void xriyl(){ /* XOR A,IYl */
	xor(*cpuIYlreg);
}
void cpixh(){ /* CP A,IXh */
	cp(*cpuIXhreg);
}
void cpixl(){ /* CP A,IXl */
	cp(*cpuIXlreg);
}
void cpiyh(){ /* CP A,IYh */
	cp(*cpuIYhreg);
}
void cpiyl(){ /* CP A,IYl */
	cp(*cpuIYlreg);
}
void inixh(){ /* INC IXh */
	uint8_t tmp = *cpuIXhreg;
	(*cpuIXhreg)++;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIXhreg & 0x80) | ((!*cpuIXhreg) << 6) | (((tmp & 0x0f) == 0x0f) << 4) | ((tmp == 0x7f) << P_FLAG));
}
void iniyh(){ /* INC IYh */
	uint8_t tmp = *cpuIYhreg;
	(*cpuIYhreg)++;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIYhreg & S_FLAG) | ((!*cpuIYhreg) << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_FLAG) | ((tmp == 0x7f) << P_FLAG));
}
void inixl()	{ /* INC IXl */
	uint8_t tmp = *cpuIXlreg;
	(*cpuIXlreg)++;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIXlreg & S_FLAG) | ((!*cpuIXlreg) << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_FLAG) | ((tmp == 0x7f) << P_FLAG));
}
void iniyl()	{ /* INC IYl */
	uint8_t tmp = *cpuIYlreg;
	(*cpuIYlreg)--;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIYlreg & S_FLAG) | ((!*cpuIYlreg) << Z_SHIFT) | (((tmp & 0x0f) == 0x0f) << H_FLAG) | ((tmp == 0x7f) << P_FLAG));
}
void dcixh()	{ /* DEC IXh */
	uint8_t tmp = *cpuIXhreg;
	(*cpuIXhreg)--;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIXhreg & S_FLAG) | ((!*cpuIXhreg) << Z_SHIFT) | ((!(tmp & 0x0f)) << H_FLAG) | ((tmp == 0x80) << P_FLAG) | N_FLAG);
}
void dciyh()	{ /* DEC IYh */
	uint8_t tmp = *cpuIYhreg;
	(*cpuIYhreg)--;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIYhreg & S_FLAG) | ((!*cpuIYhreg) << Z_SHIFT) | ((!(tmp & 0x0f)) << H_FLAG) | ((tmp == 0x80) << P_FLAG) | N_FLAG);
}
void dcixl()	{ /* DEC IXl */
	uint8_t tmp = *cpuIXlreg;
	(*cpuIXlreg)--;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIXlreg & S_FLAG) | ((!*cpuIXlreg) << Z_SHIFT) | ((!(tmp & 0x0f)) << H_FLAG) | ((tmp == 0x80) << P_FLAG) | N_FLAG);
}
void dciyl()	{ /* DEC IYl */
	uint8_t tmp = *cpuIYlreg;
	(*cpuIYlreg)--;
	*cpuFreg = ((*cpuFreg & YXC_FLAG) | (*cpuIYlreg & S_FLAG) | ((!*cpuIYlreg) << Z_SHIFT) | ((!(tmp & 0x0f)) << H_FLAG) | ((tmp == 0x80) << P_FLAG) | N_FLAG);
}

void unp(){ /* unprefix a prefixed opcode */
	cpuPC--;
}
void noop(){
printf("Illegal opcode: %02x %02x %02x %02x\n",*read_z80_memory(cpuPC-4),*read_z80_memory(cpuPC-3),*read_z80_memory(cpuPC-2),op);
exit(1);}

void power_reset () {
halted = 0;
iff1 = iff2 = cpuPC = iMode = cpuI = cpuR = 0;
cpuAF = cpuBC = cpuDE = cpuHL = cpuIX = cpuIY = 0xffff;
/* TODO: this is platform specific assignment (endianness) */
cpuAreg = (uint8_t *)&cpuAF+1;
cpuFreg = (uint8_t *)&cpuAF;
cpuBreg = (uint8_t *)&cpuBC+1;
cpuCreg = (uint8_t *)&cpuBC;
cpuDreg = (uint8_t *)&cpuDE+1;
cpuEreg = (uint8_t *)&cpuDE;
cpuHreg = (uint8_t *)&cpuHL+1;
cpuLreg = (uint8_t *)&cpuHL;
cpuIXhreg = (uint8_t *)&cpuIX+1;
cpuIXlreg = (uint8_t *)&cpuIX;
cpuIYhreg = (uint8_t *)&cpuIY+1;
cpuIYlreg = (uint8_t *)&cpuIY;
cpuAFreg = (uint16_t *)&cpuAF;
cpuBCreg = (uint16_t *)&cpuBC;
cpuDEreg = (uint16_t *)&cpuDE;
cpuHLreg = (uint16_t *)&cpuHL;
cpuIXreg = (uint16_t *)&cpuIX;
cpuIYreg = (uint16_t *)&cpuIY;
r[0] = cpuBreg;
r[1] = cpuCreg;
r[2] = cpuDreg;
r[3] = cpuEreg;
r[4] = cpuHreg;
r[5] = cpuLreg;
r[6] = NULL;
r[7] = cpuAreg;
rp[0] = cpuBCreg;
rp[1] = cpuDEreg;
rp[2] = cpuHLreg;
rp[3] = &cpuSP;
rp2[0] = cpuBCreg;
rp2[1] = cpuDEreg;
rp2[2] = cpuHLreg;
rp2[3] = cpuAFreg;
}

void interrupt_polling() {
/* mode 1: set cpuPC to 0x38 */
}

uint8_t parcalc(uint8_t val){
    val^=val>>8;
    val^=val>>4;
    val^=val>>2;
    val^=val>>1;
	return (!(val & 1) << 2);
}
