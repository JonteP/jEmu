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
void run_6502(void), interrupt_handle(interrupt_t);
void (*_6502_synchronize)(int);
void (*_6502_addcycles)(uint8_t);
uint8_t (*_6502_cpuread)(uint16_t);
void (*_6502_cpuwrite)(uint16_t, uint8_t);

extern uint_fast8_t dummywrite, irqPulled, nmiPulled, cpuStall;
extern uint_fast8_t cpuA, cpuX, cpuY, cpuP, cpuS;
extern uint16_t cpuPC;
extern uint32_t cpucc;

#endif
