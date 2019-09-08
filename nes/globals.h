#ifndef GLOBALS_H_
#define GLOBALS_H_
#include <stdint.h>

#define PRG_BANK 0x1000
#define CHR_BANK 0x400

extern uint_fast8_t  ctrb, ctrb2, ctr1, ctr2, nmiPulled, openBus;
extern uint_fast8_t  quit;
extern uint16_t vdp_wait, apu_wait;
extern int32_t ppucc;
extern const float originalFps, originalCpuClock, cyclesPerFrame;
extern float fps, cpuClock;

void save_state(), load_state();

static inline void bitset(uint_fast8_t * inp, uint_fast8_t val, uint_fast8_t b)
{
	val ? (*inp = *inp | (1 << b)) : (*inp = *inp & ~(1 << b));
}

#endif
