#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

typedef enum {
	CHR_RAM,
	CHR_ROM
} chrtype_t;
void init_mapper(void),
	 (*irq_cpu_clocked)(void), (*irq_ppu_clocked)(void),
	 (*write_mapper_register)(uint16_t, uint8_t);
void prg_bank_switch(), chr_bank_switch(), nametable_mirroring(uint8_t);
uint8_t (*read_mapper_register)(uint16_t), namco163_read(uint16_t);
float vrc6_sound(void);
float (*expansion_sound)(void);
extern uint8_t mapperInt, expSound, wramBit, wramBitVal, extendedPrg;
uint8_t mapperRead;

extern chrtype_t chrSource[0x8];
void vrc5_reset();
void mmc1_reset();
void mmc2_reset();
void mmc3_reset();

#endif
