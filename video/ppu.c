/*Versions:
 *
 * Ricoh RP2C02 - NTSC version
 * Ricoh RP2C07 - PAL version
 *
 * TODO:
 * -remove dependencies
 * -proper ntsc/pal video simulation
 * -best fake palette?
 */

#include "ppu.h"
#include <stdio.h>
#include <stdint.h>
#include "../nes/mapper.h" //CHR_RAM; chrSource; mapperInt
#include "../my_sdl.h" //render_frame
#include "../cpu/6502.h" //irqPulled
#include "../nes/nescartridge.h" //cart

struct ppuDisplayMode ntscMode = { 256, 240, NTSC_SCANLINES };
struct ppuDisplayMode  palMode = { 256, 240,  PAL_SCANLINES };

 int16_t ppudot;
 int16_t ppu_vCounter;
static uint8_t vblank_period;
static uint8_t nmiSuppressed;
static uint8_t secOam[0x20];
static uint32_t *ppuScreenBuffer = NULL;

//PPU internal registers
static uint8_t  ppuW;
static uint8_t  ppuX;
static uint16_t ppuT;
static uint16_t ppuV;

//PPU external registers
static uint8_t ppuController;
static uint8_t ppuMask;
static uint8_t ppuData;
static uint8_t ppuStatusNmi;
static uint8_t ppuStatusSpriteZero;
static uint8_t ppuStatusOverflow;
static uint8_t ppuStatusNmiDelay;

//Hard coded "fake" palettes below; TODO: make palettes selectable
//http://www.firebrandx.com/nespalette.html for more palettes

//This is Firebrandx's premiere final palette work for the NTSC NES experience.
//Combines reverse engineered NTSC output levels with a few slight tweaks to improve 'nostalgic' performance on digital displays.
//Has a wide dynamic range of dark-to-light colors.
static const uint8_t smoothFbx[] = {
    0x6A, 0x6D, 0x6A, 0x00, 0x13, 0x80, 0x1E, 0x00, 0x8A, 0x39, 0x00, 0x7A,
    0x55, 0x00, 0x56, 0x5A, 0x00, 0x18, 0x4F, 0x10, 0x00, 0x3D, 0x1C, 0x00,
    0x25, 0x32, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x40, 0x00, 0x00, 0x39, 0x24,
    0x00, 0x2E, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 ,0x00, 0x00,
    0xB9, 0xBC, 0xB9, 0x18, 0x50, 0xC7, 0x4B, 0x30, 0xE3, 0x73, 0x22, 0xD6,
    0x95, 0x1F, 0xA9, 0x9D, 0x28, 0x5C, 0x98, 0x37, 0x00, 0x7F, 0x4C, 0x00,
    0x5E, 0x64, 0x00, 0x22, 0x77, 0x00, 0x02, 0x7E, 0x02, 0x00, 0x76, 0x45,
    0x00, 0x6E, 0x8A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0x68, 0xA6, 0xFF, 0x8C, 0x9C, 0xFF, 0xB5, 0x86, 0xFF,
    0xD9, 0x75, 0xFD, 0xE3, 0x77, 0xB9, 0xE5, 0x8D, 0x68, 0xD4, 0x9D, 0x29,
    0xB3, 0xAF, 0x0C, 0x7B, 0xC2, 0x11, 0x55, 0xCA, 0x47, 0x46, 0xCB, 0x81,
    0x47, 0xC1, 0xC5, 0x4A, 0x4D, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xCC, 0xEA, 0xFF, 0xDD, 0xDE, 0xFF, 0xEC, 0xDA, 0xFF,
    0xF8, 0xD7, 0xFE, 0xFC, 0xD6, 0xF5, 0xFD, 0xDB, 0xCF, 0xF9, 0xE7, 0xB5,
    0xF1, 0xF0, 0xAA, 0xDA, 0xFA, 0xA9, 0xC9, 0xFF, 0xBC, 0xC3, 0xFB, 0xD7,
    0xC4, 0xF6, 0xF6, 0xBE, 0xC1, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//The following table was generated using blargg's Full Palette demo
static const uint8_t colblargg[] = {
     84,  84,  84,   0,  30, 116,   8,  16, 144,  48,   0, 136,
     68,   0, 100,  92,   0,  48,  84,   4,   0,  60,  24,   0,
     32,  42,   0,   8,  58,   0,   0,  64,   0,   0,  60,   0,
      0,  50,  60,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    152, 150, 152,   8,  76, 196,  48,  50, 236,  92,  30, 228,
    136,  20, 176, 160,  20, 100, 152,  34,  32, 120,  60,   0,
     84,  90,   0,  40, 114,   0,   8, 124,   0,   0, 118,  40,
      0, 102, 120,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    236, 238, 236,  76, 154, 236, 120, 124, 236, 176,  98, 236,
    228,  84, 236, 236,  88, 180, 236, 106, 100, 212, 136,  32,
    160, 170,   0, 116, 196,   0,  76, 208,  32,  56, 204, 108,
     56, 180, 204,  60,  60,  60,   0,   0,   0,   0,   0,   0,
    236, 238, 236, 168, 204, 236, 188, 188, 236, 212, 178, 236,
    236, 174, 236, 236, 174, 212, 236, 180, 176, 228, 196, 144,
    204, 210, 120, 180, 222, 120, 168, 226, 144, 152, 226, 180,
    160, 214, 228, 160, 162, 160,   0,   0,   0,   0,   0,   0
};

static inline void check_nmi();
static inline void horizontal_t_to_v();
static inline void vertical_t_to_v();
static inline void ppu_render();
static inline void reload_tile_shifter();
static inline void toggle_a12(uint16_t);
static inline void ppuwrite(uint16_t, uint8_t);
static inline uint8_t * ppuread(uint16_t);
static inline void none(), seZ(), seRD(), seWD(), seRR(), seWW(), tfNT(), tfAT(), tfLT(), tfHT(), sfNT(), sfAT(), sfLT(), sfHT(), dfNT(), hINC(), vINC();

//TODO: what are proper startup values?
void init_ppu() {
    free(ppuScreenBuffer);
    ppuScreenBuffer = malloc(ppuCurrentMode->height * ppuCurrentMode->width * sizeof(uint32_t));
    frame = 0;
    nmiFlipFlop = 0;
    ppucc = 0;
    ppuStatusNmi = 0;
    ppuStatusSpriteZero = 0;
    ppuStatusOverflow = 0;
    ppuStatusNmiDelay = 0;
    ppudot = 0;
    ppu_vCounter = 0;
    vblank_period = 0;
    nmiSuppressed = 0;
    ppuW = 0;
    ppuX = 0;
}

void run_ppu (uint16_t ntimes) {
    static void (*spriteEvaluation[0x341])() = {
        seZ,  seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD,
        seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD,
        seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD,
        seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD, seWD, seRD,
        seWD, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR, seWW, seRR,
        seWW, none, none, none, none, none, none, none, none, none, none, none, none, none, none, none,
        none, none, none, none, none, none, none, none, none, none, none, none, none, none, none, none,
        none, none, none, none, none, none, none, none, none, none, none, none, none, none, none, none,
        none, none, none, none, none, none, none, none, none, none, none, none, none, none, none, none,
        none, none, none, none, none, none, none, none, none, none, none, none, none, none, none, none,
        none, none, none, none, none
    };

    static void (*fetchGraphics[0x341])() = {
        none, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        vINC, sfNT, none, sfAT, none, sfLT, none, sfHT, none, sfNT, none, sfAT, none, sfLT, none, sfHT,
        none, sfNT, none, sfAT, none, sfLT, none, sfHT, none, sfNT, none, sfAT, none, sfLT, none, sfHT,
        none, sfNT, none, sfAT, none, sfLT, none, sfHT, none, sfNT, none, sfAT, none, sfLT, none, sfHT,
        none, sfNT, none, sfAT, none, sfLT, none, sfHT, none, sfNT, none, sfAT, none, sfLT, none, sfHT,
        none, tfNT, none, tfAT, none, tfLT, none, tfHT, hINC, tfNT, none, tfAT, none, tfLT, none, tfHT,
        hINC, dfNT, none, dfNT, none
    };

    while (ntimes) {
        if (mapperInt && !irqPulled) {
            irqPulled = 1;
        }
        ppudot++;
        ppucc++;
        check_nmi();
        if (ppudot == 341) {
            ppu_vCounter++;
            ppudot = 0;
        }
        if (ppu_vCounter == ppuCurrentMode->scanlines) {
            ppu_vCounter = 0;
            frame++;
        }

//VBLANK ONSET
        if (ppu_vCounter == 241 && ppudot == 1) {
            ppuStatusNmi = 1; /* set vblank */
            vblank_period = 1;
        }
//PRERENDER SCANLINE
        else if (ppu_vCounter == (ppuCurrentMode->scanlines - 1)) {
            if (ppuMask & 0x18)	{
                (*fetchGraphics[ppudot])();
                (*spriteEvaluation[ppudot])();
            }
            ppu_render();
            if (ppudot == 1) {
                if (ppuStatusNmi) {
                    ppuStatusNmi = 0; /* clear vblank */
                    ppuStatusNmiDelay = 1;
                }
                nmiSuppressed = 0;
                ppuStatusSpriteZero = 0;
                nmiPulled = 0;
                vblank_period = 0;
                ppuStatusOverflow = 0;
            }
            if (ppudot ==2)
                ppuStatusNmiDelay = 0;
            else if (ppudot >= 257 && ppudot <= 320) {
//(reset OAM)
                ppuOamAddress = 0; /* only if rendering active? */
                if (ppudot == 257)
                    horizontal_t_to_v();
                else if (ppudot >= 280 && ppudot <= 304)
                    vertical_t_to_v();
            } else if (ppudot == 339) {
                if (frame%2 && (ppuMask & 0x18) && ppuCurrentMode->scanlines == NTSC_SCANLINES)
                    ppudot++;
            }
//RESET CLOCK COUNTER HERE...
        } else if (ppu_vCounter == 240 && ppudot == 0) {
            ppucc = 0;
        }
//RENDERED LINES
        else if (ppu_vCounter < 240) {
            if (!ppu_vCounter && !ppudot)
                render_frame(ppuScreenBuffer);
            if (ppuMask & 0x18)	{
                (*fetchGraphics[ppudot])();
                (*spriteEvaluation[ppudot])();
            }
            ppu_render();
            if (ppudot == 257)
                horizontal_t_to_v();
        }
        ntimes--;
    }
}

static uint8_t ntData, attData, tileLow, tileHigh, spriteLow, spriteHigh;
static uint16_t tileShifterLow, tileShifterHigh, attShifterHigh, attShifterLow;
static uint8_t oamOverflow1, oamOverflow2, nSprite1, nSprite2, nData, data, nData2;
static uint8_t spriteBuffer[256], zeroBuffer[256], priorityBuffer[256], isSpriteZero = 0xff;
static uint8_t foundSprites = 0;

void none () {}

void seZ () {
    isSpriteZero = 0xff;
    oamOverflow1 = 0;
    oamOverflow2 = 0;
    nSprite1 = 0;
    nSprite2 = 0;
    nData = 0;
    nData2 = 0;
    foundSprites = 0;
}

void seRD () {
    data = 0xff;
}

void seWD () {
    secOam[(ppudot >> 1) - 1] = data;
}

void seRR () {
    if (ppudot == 65)
        nSprite1 = (ppuOamAddress >> 2);
    if (nSprite1 == 64) {
        oamOverflow1 = 1;
        nSprite1 = 0;
        ppuOamAddress = 0;
    }
    data = oam[(nSprite1 << 2) + nData]; /* read y coordinate */
    if (!nData && !(data <= ppu_vCounter && ppu_vCounter <= (data + 7 + ( (ppuController >> 2) & 0x08)))) { /* not within range */
        nSprite1++;
        ppuOamAddress += 4;
    }
    else {
        if (!nData  && !oamOverflow1) {
            foundSprites++;
            if (foundSprites > 8)
                ppuStatusOverflow = 1;
        }
        nData++;
        if (nData == 4)	{
            nData = 0;
            nSprite1++;
            ppuOamAddress += 4;
        }
    }
}

void seWW () {
    if (!(oamOverflow1 | oamOverflow2))	{
        secOam[(nSprite2 << 2) + nData2] = data;
        if (nData2 == 3) {
            nData2 = 0;
            nSprite2++;
            if (nSprite2 == 8) {
                nSprite2 = 0;
                oamOverflow2 = 1;
            }
        }
        else {
            if (nData2 == 1 && nSprite1 == 0)
                isSpriteZero = nSprite2;
            nData2 = nData;
        }
    }
}

void tfNT() {
    uint16_t a = (0x2000 | (ppuV & 0xfff));
    ntData = *ppuread(a);
}

void tfAT() {
    uint16_t a = (0x23c0 | (ppuV & 0xc00) | ((ppuV >> 4) & 0x38) | ((ppuV >> 2) & 0x07));
    uint8_t bgData = *ppuread(a);
    attData = ((bgData >> ((((ppuV >> 1) & 1) | ((ppuV >> 5) & 2)) << 1)) & 3);
}

void tfLT() {
    uint16_t a = ((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7));
    tileLow = *ppuread(a);
}

void tfHT() {
    uint16_t a = ((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7) + 8);
    tileHigh = *ppuread(a);
    toggle_a12(a);
}

void sfNT() {
    ppuOamAddress = 0;
}

void sfAT() {
    ppuOamAddress = 0;
}

static uint8_t cSprite, *sprite, spriteRow;
static uint16_t patternOffset;

//Sprite fetch, low tile
void sfLT() {
    cSprite = ((ppudot >> 3) & 0x07);
    sprite = secOam + (cSprite << 2);
    uint8_t yOffset = ppu_vCounter - (sprite[0]);
    uint8_t flipY = ((sprite[2] >> 7) & 1);
    spriteRow = (yOffset & 7) + flipY * (7 - ((yOffset & 7) << 1));
    if (ppuController & 0x20) /* 8x16 sprites */
        patternOffset = (((sprite[1] & 0x01) << 12) 	// select pattern table
        + ((sprite[1] & 0xfe) << 4) 					// offset for top tile (16b per tile)
        + (((yOffset << 1) ^ (flipY << 4)) & 0x10)); 	// select bottom tile if either offset 8+ or flipped sprite
    else if(!(ppuController & 0x20)) // 8x8 sprites
        patternOffset = (sprite[1] << 4) + ((ppuController & 0x08) ? 0x1000 : 0);
    spriteLow = *ppuread(patternOffset + spriteRow);
    ppuOamAddress = 0;
}

//Sprite fetch, high tile
void sfHT() {
    spriteHigh = *ppuread(patternOffset + spriteRow + 8);
    ppuOamAddress = 0;
    uint8_t flipX = ((sprite[2] >> 6) & 1);
    uint8_t nPalette = (sprite[2] & 3);
	/* decode pattern data and store in buffer */
    for(int pcol = 0; pcol < 8; pcol++) {
        uint8_t spritePixel = (7 - pcol - flipX * (7 - (pcol << 1)));
        uint8_t pixelData = (((spriteLow >> spritePixel) & 0x01) + (((spriteHigh >> spritePixel) & 0x01) << 1));
        toggle_a12(patternOffset + spriteRow);
        if(pixelData && (spriteBuffer[sprite[3] + pcol])==0xff && !(sprite[3] == 0xff)) {/* TODO: this PPU access probably should not be here, screws up mmc3 irq? */
            spriteBuffer[sprite[3] + pcol] = *ppuread(0x3f10 + (nPalette << 2) + pixelData);
            if (isSpriteZero == cSprite) {
                zeroBuffer[sprite[3] + pcol] = 1;
            }
            priorityBuffer[sprite[3] + pcol] = (sprite[2] & 0x20);
        }
    }
}

void dfNT() { }

void hINC() {
    if (ppuMask & 0x18)	{
        if ((ppuV & 0x1f) == 0x1f) {
            ppuV &= ~0x1f;
            ppuV ^= 0x0400; /* switch nametable horizontally */
        }
        else
            ppuV++; /* next tile */
    }
}

void vINC() {
    if (ppuMask & 0x18)	{
        if ((ppuV & 0x7000) != 0x7000)
            ppuV += 0x1000;
        else {
            ppuV &= ~0x7000;
            uint8_t coarseY = ((ppuV & 0x3e0) >> 5);
            if (coarseY == 29) {
                coarseY = 0;
                ppuV ^= 0x0800;
            } else if (coarseY == 31)
                coarseY = 0;
            else
                coarseY++;
            ppuV = (ppuV & ~0x03e0) | (coarseY << 5);
        }
    }
}

void reload_tile_shifter() {
    tileShifterLow = ((tileShifterLow & 0xff00) | tileLow);
    tileShifterHigh = ((tileShifterHigh & 0xff00) | tileHigh);
    attShifterLow = ((attShifterLow & 0xff00) | ((attData & 1) ? 0xff : 0x00));
    attShifterHigh = ((attShifterHigh & 0xff00) | ((attData & 2) ? 0xff : 0x00));
}

void ppu_render() {
    uint8_t color;
/* save for render
 * color mask
 *
 * The shifters shift at h=2, the palette address changes at h=3 for the palette lookup, and the pixel is drawn during h=4 (as seen by vid_ changing).
 * It also looks like the shift registers are actually reloaded at h=9,17,25,... instead of at h=8,16,24,
 */
    if (ppudot >= 1 && ppudot <= 257) { /* tile data fetch */
        if (ppudot <=256) {
            if (ppudot%8 == 2)
                reload_tile_shifter();
        }
        if (ppudot >=2) {
            uint8_t pValue;
            int16_t cDot = ppudot - 2;
            pValue = ((attShifterHigh >> (12 - ppuX)) & 8) | ((attShifterLow >> (13 - ppuX)) & 4) | ((tileShifterHigh >> (14 - ppuX)) & 2) | ((tileShifterLow >> (15 - ppuX)) & 1);
            uint8_t bgColor = (!(ppuMask & 0x18) && (ppuV & 0x3f00) == 0x3f00) ? *ppuread(ppuV & 0x3fff) : *ppuread(0x3f00);
            uint8_t isSprite = (spriteBuffer[cDot]!=0xff && (ppuMask & 0x10) && ((cDot > 7) || (ppuMask & 0x04)));
            uint8_t isBg = ((pValue & 0x03) && (ppuMask & 0x08) && ((cDot > 7) || (ppuMask & 0x02)));
            if (ppu_vCounter < 240) {
                if (isSprite && isBg) {
                    if (!ppuStatusSpriteZero && zeroBuffer[cDot] && cDot<255) {
                        ppuStatusSpriteZero = 1;
                        color = 0x10;
                    }
                    color = priorityBuffer[cDot] ?  *ppuread(0x3f00 + pValue) : spriteBuffer[cDot];
                }
                else if (isSprite && !isBg)
                    color = spriteBuffer[cDot];
                else if (isBg && !isSprite)
                    color = *ppuread(0x3f00 + pValue);
                else
                    color = bgColor;
                ppuScreenBuffer[ppu_vCounter * ppuCurrentMode->width + cDot] = (0xff000000 | (smoothFbx[color * 3] << 16) | (smoothFbx[(color * 3) + 1] << 8) | smoothFbx[(color * 3) + 2]);
            }
            tileShifterHigh <<= 1;
            tileShifterLow <<= 1;
            attShifterHigh <<= 1;
            attShifterLow <<= 1;
            if (ppudot == 257) {
                nSprite2 = 0;
                memset(spriteBuffer,0xff,256);
                memset(priorityBuffer,0,256);
                memset(zeroBuffer,0,256);
            }
        }
    }
    else if (ppudot >= 321 && ppudot <= 336) { /* prefetch tiles */
        if (ppudot%8 == 1)
            reload_tile_shifter();
        tileShifterHigh <<= 1;
        tileShifterLow <<= 1;
        attShifterHigh <<= 1;
        attShifterLow <<= 1;
    }
}

void horizontal_t_to_v() {
    if (ppuMask & 0x18) {
        ppuV = (ppuV & 0xfbe0) | (ppuT & 0x41f); /* reset x scroll */
    }
}

void vertical_t_to_v() {
    if (ppuMask & 0x18)	{
        ppuV = (ppuV & 0x841f) | (ppuT & 0x7be0); /* reset Y scroll */
    }
}

static uint8_t ppureg = 0, vbuff = 0;
uint8_t read_ppu_register(uint16_t addr) {
    unsigned int tmpval8;
    switch (addr & 0x2007) {
    case 0x2002:
        if (ppucc == 343 || ppucc == 344) {/* suppress if read and set at same time */
            nmiSuppressed = 1;
            nmiFlipFlop = 0;
        } else if (ppucc == 342) {
            ppuStatusNmi = 0;
        }
        tmpval8 = ((ppuStatusNmi | ppuStatusNmiDelay)  <<7) | (ppuStatusSpriteZero<<6) | (ppuStatusOverflow<<5) | (ppureg & 0x1f);
        ppuStatusNmi = 0;
        ppuStatusNmiDelay = 0;
        ppuW = 0;
        break;
    case 0x2004:
/* TODO: Correct behavior when accessed during rendering */
        if ((ppuMask & 0x18) && !vblank_period) {
            if (ppudot < 65)
                tmpval8 = 0xff;
            else if (ppudot <257)
                tmpval8 = secOam[(nSprite2 << 2) + nData2];
            else if (ppudot < 321) {
                cSprite = ((ppudot >> 3) & 0x07);
                tmpval8 = secOam[cSprite << 2];
            } else
                tmpval8 = oam[ppuOamAddress];
        }
        else
            tmpval8 = oam[ppuOamAddress];
        break;
    case 0x2007:
        if (ppuV < 0x3f00) {
            tmpval8 = vbuff;
            vbuff = *ppuread(ppuV);
        }
/* TODO: buffer update when reading palette */
        else if ((ppuV) >= 0x3f00) {
            tmpval8 = *ppuread(ppuV) & ((ppuMask & 0x01) ? 0x30 : 0xff);
            vbuff = tmpval8;
        }
        ppuV += (ppuController & 0x04) ? 32 : 1;
        toggle_a12(ppuV);
        break;
    }
    return tmpval8;
}

void write_ppu_register(uint16_t addr, uint8_t tmpval8) {
    ppureg = tmpval8;
    switch (addr & 0x2007) {
    case 0x2000:
        ppuController = tmpval8;
        ppuT &= 0xf3ff;
        ppuT |= ((ppuController & 3)<<10);
        if (!(ppuController & 0x80)) {
            nmiFlipFlop = 0;
            nmiSuppressed = 0;
            nmiPulled = 0;
        } else if (ppuController & 0x80) {
            check_nmi();
        }
        break;
    case 0x2001:
        ppuMask = tmpval8;
        break;
    case 0x2002:
        break;
    case 0x2003:
        ppuOamAddress = tmpval8;
        break;
    case 0x2004:
        ppureg = tmpval8;
/* TODO: writing during rendering */
        if (vblank_period || !(ppuController & 0x18)) {
            oam[ppuOamAddress++] = tmpval8;
        }
        break;
    case 0x2005:
        if (ppuW == 0) {
            ppuT &= 0xffe0;
            ppuT |= ((tmpval8 & 0xf8)>>3); /* coarse X scroll */
            ppuX = (tmpval8 & 0x07); /* fine X scroll  */
            ppuW = 1;
        } else if (ppuW == 1) {
            ppuT &= 0x8c1f;
            ppuT |= ((tmpval8 & 0xf8)<<2); /* coarse Y scroll */
            ppuT |= ((tmpval8 & 0x07)<<12); /* fine Y scroll */
            ppuW = 0;
        }
        break;
    case 0x2006:
        if (ppuW == 0) {
            ppuT &= 0x80ff;
            ppuT |= ((tmpval8 & 0x3f) << 8);
            ppuW = 1;
        } else if (ppuW == 1) {
            ppuT &= 0xff00;
            ppuT |= tmpval8;
            ppuV = ppuT;
            toggle_a12(ppuV);
            ppuW = 0;
        }
        break;
    case 0x2007:
        if (vblank_period || !(ppuMask & 0x18)) {
            ppuData = tmpval8;
            if ((ppuV & 0x3fff) >= 0x3f00)
                ppuwrite((ppuV & 0x3fff), (ppuData & 0x3f));
            else {
                ppuwrite((ppuV & 0x3fff), ppuData);
            }
            ppuV += (ppuController & 0x04) ? 32 : 1;
            toggle_a12(ppuV);
            break;
        } else {
            vINC();
            hINC();
        }
    }
}

uint8_t * ppuread(uint16_t address) {
    if (address < 0x2000) { //pattern tables
        return ppu_read_chr(address);
    } else if (address < 0x3f00) { //nametables
        return ppu_read_nt(address);
    }
    else if (address >= 0x3f00) { /* palette RAM */
        if (address == 0x3f10)
            address = 0x3f00;
        else if (address == 0x3f14)
            address = 0x3f04;
        else if (address == 0x3f18)
            address = 0x3f08;
        else if (address == 0x3f1c)
            address = 0x3f0c;
        return &palette[(address & 0x1f)];
    }
    return 0;
}

void ppuwrite(uint16_t address, uint8_t value) {
    if (address < 0x2000) { /* pattern tables */
        if (chrSource[(address >> 10)] == CHR_RAM)
            chrSlot[(address >> 10)][address & 0x3ff] = value;
    } else if (address >= 0x2000 && address < 0x3f00) { /* nametables */
        ppu_write_nt(address,value);
    }
    else if (address >= 0x3f00) { /* palette RAM */
        if (address == 0x3f10)
            address = 0x3f00;
        else if (address == 0x3f14)
            address = 0x3f04;
        else if (address == 0x3f18)
            address = 0x3f08;
        else if (address == 0x3f1c)
            address = 0x3f0c;
        palette[(address & 0x1f)] = value;
    }
}

void check_nmi() {
    if ((ppuController & 0x80) && ppuStatusNmi && !nmiPulled && !nmiSuppressed)	{
        nmiPulled = 1;
        nmiFlipFlop = ppucc;
    }
}

uint16_t lastAddress = 0x0000;
void toggle_a12(uint16_t address) {
    if ((address & 0x1000) && ((address ^ lastAddress) & 0x1000) && (!strcmp(cart.slot,"txrom") || !strcmp(cart.slot,"tqrom") || !strcmp(cart.slot,"txsrom") || !strcmp(cart.slot,"tc0190fmcp"))) {
        irq_ppu_clocked();
    }
    else if (!(address & 0x1000) && ((address ^ lastAddress) & 0x1000) && !strcmp(cart.slot,"txrom")) {
    }
/* during sprite fetches, the PPU rapidly alternates between $1xxx and $2xxx, and the MMC3 does not see A13 -
 * as such, the PPU will send 8 rising edges on A12 during the sprite fetch portion of the scanline
 * (with 8 pixel clocks, or 2.67 CPU cycles between them */
    lastAddress = address;
}
