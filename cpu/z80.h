#ifndef Z80_H_
#define Z80_H_
#include <stdio.h>
#include <stdint.h>

void run_z80(void), z80_power_reset(void);

// Function pointers to be defined by the emulated machine
uint8_t * (*read_z80_memory)(uint16_t);
void (*write_z80_memory)(uint16_t, uint8_t);
uint8_t (*read_z80_register)(uint8_t);
void (*write_z80_register)(uint8_t, uint8_t);
void (*z80_addcycles)(uint8_t);
void (*z80_synchronize)(int);

extern uint8_t z80_irqPulled, z80_nmiPulled;

#endif /* Z80_H_ */
