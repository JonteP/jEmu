#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

typedef enum {
	CHR_RAM,
	CHR_ROM
} chrtype_t;
void init_mapper(void), vrc_irq(void), mmc3_irq(void), ss88006_irq(),
	 (*irq_cpu_clocked)(void), (*irq_ppu_clocked)(void),
	 (*write_mapper_register4)(uint16_t, uint8_t),
	 (*write_mapper_register6)(uint16_t, uint8_t),
	 (*write_mapper_register8)(uint16_t, uint8_t);
void prg_bank_switch(), chr_bank_switch(), nametable_mirroring(uint8_t);
uint8_t (*read_mapper_register)(uint16_t), namco163_read(uint16_t);
float vrc6_sound(void);
float (*expansion_sound)(void);
extern uint8_t mapperInt, expSound, wramBit, wramBitVal, extendedPrg;
uint8_t mapperRead, ntTarget;
uint8_t read_vrc5_chrrom(uint16_t);
void write_vrc5_qtram(uint16_t, uint8_t);
extern chrtype_t chrSource[0x8];

#endif
