#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include <stdint.h>

typedef enum mirrorMode {
    H_MIRROR = 0,
    V_MIRROR = 1,
    ONELOW_MIRROR = 2,
    ONEHIGH_MIRROR = 3,
} MirrorMode;

typedef struct gameInfos_ {
	char *title;
	char *year;
	char *publisher;
	char *serial;
} gameInfos;

typedef struct gameFeatures_ {
	char slot[20];
	char pcb[30];
	MirrorMode mirroring;
	uint8_t pSlots;
	uint8_t cSlots;
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

void nes_load_rom(), nes_close_rom();

#endif /* CARTRIDGE_H_ */
