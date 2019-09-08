#ifndef C6502_H_
#define C6502_H_
#include <stdint.h>

typedef enum {
    IRQ,
    NMI,
	BRK
} interrupt_t;

typedef enum {
    HARD_RESET = 1,
    SOFT_RESET = 2,
	NONE = 0
} reset_t;

extern reset_t rstFlag;
void opdecode(void), interrupt_handle(interrupt_t), synchronize(uint_fast8_t);
uint_fast8_t cpuread(uint16_t);

extern uint_fast8_t dummywrite, irqPulled, nmiPulled, cpuStall;
extern uint_fast8_t *prgSlot[0x8], cpuRam[0x800];
extern uint_fast8_t cpuA, cpuX, cpuY, cpuP, cpuS;
extern uint16_t cpuPC;
extern uint32_t cpucc;

#endif
