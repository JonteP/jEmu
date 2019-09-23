#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define DOTS_PER_SCANLINE	341
#define NTSC_SCANLINES		262
#define PAL_SCANLINES		312

struct ppuDisplayMode {
	uint16_t width;
	uint16_t height;
	uint16_t scanlines;
};

typedef enum version {
	PPU_NTSC = 0,
	PPU_PAL	= 1
} PPU_Version;

void write_ppu_register(uint_fast16_t, uint_fast8_t), draw_nametable(), draw_pattern(), draw_palette(), init_ppu();
uint_fast8_t read_ppu_register(uint_fast16_t), ppu_read(uint_fast16_t);
void run_ppu(uint_fast16_t);

extern uint_fast8_t ppuW, ppuX;
extern uint_fast16_t ppuT, ppuV;
extern uint_fast8_t ciram[0x800], palette[0x20];
extern uint_fast8_t *chrSlot[0x8], *nameSlot[0x4], oam[0x100];
extern uint_fast8_t throttle, ppuOamAddress;
extern int16_t ppudot, ppu_vCounter;
extern uint32_t nmiFlipFlop, frame, *ppuScreenBuffer;
extern struct ppuDisplayMode ntscMode, palMode, *ppuCurrentMode;
#endif
