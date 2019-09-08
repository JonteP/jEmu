#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include <stdint.h>

typedef struct gameInfos_ {
	char *title;
	char *year;
	char *publisher;
	char *serial;
} gameInfos;
typedef struct gameFeatures_ {
	char slot[20];
	char pcb[30];
	uint_fast8_t mirroring;
	uint_fast8_t pSlots;
	uint_fast8_t cSlots;
	long chrSize;
	long prgSize;
	long wramSize;
	long bwramSize;
	long cramSize;
	long vrc24Prg1;
	long vrc24Prg0;
	long vrc24Chr;
	long vrc6Prg1;
	long vrc6Prg0;
	char mmc1_type[20];
	char mmc3_type[20];
} gameFeatures;

extern gameFeatures cart;
extern int psize, csize;
extern uint_fast8_t *prg, *chrRom, *chrRam, *bwram, *wram, *wramSource, mirroring[4][4], wramEnable;

void load_rom(), close_rom();

#endif /* CARTRIDGE_H_ */
