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

uint8_t oam[0x100];     //object attribute memory
uint8_t ciRam[0x800];
uint8_t palette[0x20];
uint8_t *chrSlot[0x8];  //maps VRAM 0x0000-0x1fff in 0x400B banks
uint8_t *nameSlot[0x4]; //maps the nametable to CIRAM and/or cartridge RAM according to mirroring mode

uint8_t ppuOamAddress;
uint32_t nmiFlipFlop;
uint32_t frame;
int32_t ppucc;
extern int16_t ppu_vCounter;
extern struct ppuDisplayMode ntscMode;
extern struct ppuDisplayMode palMode;
       struct ppuDisplayMode *ppuCurrentMode;

uint8_t* (*ppu_read_chr)(uint16_t);
uint8_t* (*ppu_read_nt)(uint16_t);
void    (*ppu_write_chr)(uint16_t, uint8_t);
void    (*ppu_write_nt)(uint16_t, uint8_t);
void    write_ppu_register(uint16_t, uint8_t);
void    init_ppu();
void    run_ppu(uint16_t);
uint8_t ppu_read(uint16_t);
uint8_t read_ppu_register(uint16_t);
#endif
