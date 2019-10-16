#ifndef NESEMU_H_
#define NESEMU_H_
#include <stdint.h>

#define PRG_BANK 0x1000
#define CHR_BANK 0x400

// CLOCKS AND DIVIDERS
#define NES_NTSC_MASTER		21477272
#define NES_PAL_MASTER		26601713
#define NTSC_CPU_CLOCK_DIV	12
#define PAL_CPU_CLOCK_DIV	16
#define NTSC_PPU_CLOCK_DIV	4
#define PAL_PPU_CLOCK_DIV	5
#define NTSC_APU_CLOCK_DIV	NTSC_CPU_CLOCK_DIV
#define PAL_APU_CLOCK_DIV	PAL_CPU_CLOCK_DIV

struct memSlot {
    uint16_t mask;
    uint8_t writable;
    uint8_t *memory;
};

extern uint_fast8_t  ctrb, ctrb2, ctr1, ctr2, nmiPulled, openBus;
extern uint_fast8_t  quit;
extern uint8_t *prgSlot[0x8], cpuRam[0x800];
extern struct memSlot *cpuMemory[0x10];
extern int32_t ppucc;
extern const float originalFps, originalCpuClock, cyclesPerFrame;
extern float fps;
extern struct machine nes_ntsc,	nes_pal, famicom, fds;

void save_state(), load_state();
int nesemu();

static inline void bitset(uint_fast8_t * inp, uint_fast8_t val, uint_fast8_t b)
{
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

#endif
