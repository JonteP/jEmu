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

uint8_t  dummywrite; //used for mmc1; TODO: cleaner solution
uint8_t  irqPulled;
uint8_t  nmiPulled;
uint32_t _6502_M2;

//function pointers to be hooked up by emulated machine
void    (*_6502_synchronize)(int);
void    (*_6502_addcycles)  (uint8_t);
uint8_t (*_6502_cpuread)    (uint16_t);
void    (*_6502_cpuwrite)   (uint16_t, uint8_t);

void run_6502(void);
void _6502_power_reset(reset_t);
#endif
