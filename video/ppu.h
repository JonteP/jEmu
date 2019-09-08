#ifndef PPU_H_
#define PPU_H_
#include <stdint.h>

#define WWIDTH 320 /*1280 */
#define WHEIGHT 240 /*1024 */
#define SWIDTH 256
#define SHEIGHT 240
#define WPOSX 100
#define WPOSY 100
#define FRAMETIME 16639261 /*60.098823055*/

void init_time(void), write_ppu_register(uint_fast16_t, uint_fast8_t), draw_nametable(), draw_pattern(), draw_palette();
uint_fast8_t read_ppu_register(uint_fast16_t), ppu_read(uint_fast16_t);
void run_ppu(uint_fast16_t);

extern uint_fast8_t ppuW, ppuX;
extern uint_fast16_t ppuT, ppuV;
extern uint_fast8_t ciram[0x800], palette[0x20];
extern uint_fast8_t *chrSlot[0x8], *nameSlot[0x4], oam[0x100],
			   frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT<<1][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
extern uint_fast8_t throttle, ppuOamAddress;
extern int16_t ppudot, vCounter;
extern uint32_t nmiFlipFlop, frame;

#endif
