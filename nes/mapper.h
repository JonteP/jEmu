#ifndef MAPPER_H_
#define MAPPER_H_
#include <stdint.h>

typedef enum {
	CHR_RAM,
	CHR_ROM
} chrtype_t;
void init_mapper(void), vrc_irq(void), mmc3_irq(void), ss88006_irq(),
	 (*irq_cpu_clocked)(void), (*irq_ppu_clocked)(void),
	 (*write_mapper_register4)(uint_fast16_t, uint_fast8_t),
	 (*write_mapper_register6)(uint_fast16_t, uint_fast8_t),
	 (*write_mapper_register8)(uint_fast16_t, uint_fast8_t);
void prg_bank_switch(), chr_bank_switch();
uint_fast8_t (*read_mapper_register)(uint_fast16_t), namco163_read(uint_fast16_t);
float vrc6_sound(void);
float (*expansion_sound)(void);
extern uint_fast8_t mapperInt, expSound, wramBit, wramBitVal, prgBank[8], chrBank[8], extendedPrg;
extern chrtype_t chrSource[0x8];

#endif /* MAPPER_H_ */
