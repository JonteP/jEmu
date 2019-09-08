#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_
#include <stdint.h>
#include "sha.h"
#include "parser.h"

#define RAM_SIZE			0x2000
#define CARTRAM_SIZE		(RAM_SIZE << 2) //maximum supported size
#define BANK_SHIFT			14
#define BANK_SIZE			(1 << BANK_SHIFT)

typedef enum _mapper{
	GENERIC,
	SEGA,
	CODEMASTERS,
	KOREA
} Mapper;

struct RomFile {
	uint8_t *rom;
	uint8_t mask;
	xmlChar *sha1;
	Mapper mapper;
	uint8_t battery;
};

extern uint8_t fcr[3], *bank[3], cartRam[CARTRAM_SIZE], memControl, bramReg, systemRam[RAM_SIZE], ioEnabled;
Mapper mapper;
extern struct RomFile cartRom, *currentRom;

struct RomFile load_rom(char *);
void init_slots(), close_rom(), memory_control(uint8_t);
uint8_t * (*read_bank0)(uint16_t),
		* (*read_bank1)(uint16_t),
		* (*read_bank2)(uint16_t),
		* (*read_bank3)(uint16_t);
void (*banking)(void);
#endif /* CARTRIDGE_H_ */
