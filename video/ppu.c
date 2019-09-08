#include "ppu.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include "globals.h"
#include "apu.h"
#include "mapper.h"
#include "my_sdl.h"
#include "6502.h"
#include "cartridge.h"

static inline void check_nmi(), horizontal_t_to_v(), vertical_t_to_v(), ppu_render(), reload_tile_shifter(), toggle_a12(uint_fast16_t), ppuwrite(uint_fast16_t, uint_fast8_t);
static inline uint_fast8_t * ppuread(uint_fast16_t);

uint_fast8_t *chrSlot[0x8], *nameSlot[0x4], oam[0x100], frameBuffer[SHEIGHT][SWIDTH], nameBuffer[SHEIGHT<<1][SWIDTH<<1], patternBuffer[SWIDTH>>1][SWIDTH], paletteBuffer[SWIDTH>>4][SWIDTH>>1];
uint_fast8_t ppuOamAddress;
uint_fast8_t throttle = 1;
int16_t ppudot = 0, vCounter = 0;
uint_fast8_t ciram[0x800], palette[0x20];
static uint_fast8_t vblank_period = 0, nmiSuppressed = 0, secOam[0x20];

/* PPU internal registers */
uint_fast8_t ppuW = 0, ppuX = 0;
uint_fast16_t ppuT, ppuV;

/* PPU external registers */
static uint_fast8_t ppuController, ppuMask, ppuData, ppuStatusNmi = 0, ppuStatusSpriteZero = 0, ppuStatusOverflow = 0;

uint32_t frame = 0, nmiFlipFlop = 0;
int32_t ppucc = 0;

static uint_fast8_t ppuStatusNmiDelay = 0;

static inline void none(), seZ(), seRD(), seWD(), seRR(), seWW(), tfNT(), tfAT(), tfLT(), tfHT(), sfNT(), sfAT(), sfLT(), sfHT(), dfNT(), hINC(), vINC();

void run_ppu (uint_fast16_t ntimes) {
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
	none, none, none, none, none };

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
	hINC, dfNT, none, dfNT, none };

	while (ntimes) {
		if (mapperInt && !irqPulled)
		{
			irqPulled = 1;
		}

		ppudot++;
		ppucc++;

		check_nmi();

		if (ppudot == 341)
		{
		vCounter++;
		ppudot = 0;
		}
		if (vCounter == 262)
		{
		vCounter = 0;
		frame++;
		}

/* VBLANK ONSET */
	if (vCounter == 241 && ppudot == 1) {
		ppuStatusNmi = 1; /* set vblank */
		vblank_period = 1;

/* PRERENDER SCANLINE */
	} else if (vCounter == 261) {
		if (ppuMask & 0x18)
		{
			(*fetchGraphics[ppudot])();
			(*spriteEvaluation[ppudot])();
		}
		ppu_render();
		if (ppudot == 1)
		{
			if (ppuStatusNmi)
			{
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
			  /* (reset OAM) */
			  ppuOamAddress = 0; /* only if rendering active? */
			  if (ppudot == 257)
				  horizontal_t_to_v();
			  else if (ppudot >= 280 && ppudot <= 304)
				  vertical_t_to_v();
		  } else if (ppudot == 339) {
				if (frame%2 && (ppuMask & 0x18))
					ppudot++;
		}

/* RESET CLOCK COUNTER HERE... */
	} else if (vCounter == 240 && ppudot == 0) {
		ppucc = 0;

/* RENDERED LINES */
	} else if (vCounter < 240)
	{
		if (!vCounter && !ppudot)
			render_frame();
		if (ppuMask & 0x18)
		{
		(*fetchGraphics[ppudot])();
		(*spriteEvaluation[ppudot])();
		}
		ppu_render();
		if (ppudot == 257)
			horizontal_t_to_v();
	}
	ntimes--;
	vdp_wait--;
	}
}
static uint_fast8_t bgData, ntData, attData, tileLow, tileHigh, spriteLow, spriteHigh;
static uint_fast16_t tileShifterLow, tileShifterHigh, attShifterHigh, attShifterLow;
static uint_fast8_t oamOverflow1, oamOverflow2, nSprite1, nSprite2, nData, data, nData2;
static uint_fast8_t spriteBuffer[256], zeroBuffer[256], priorityBuffer[256], isSpriteZero = 0xff;
static uint_fast8_t foundSprites = 0;
void none () {}
void seZ ()
{
	isSpriteZero = 0xff;
	oamOverflow1 = 0;
	oamOverflow2 = 0;
	nSprite1 = 0;
	nSprite2 = 0;
	nData = 0;
	nData2 = 0;
	foundSprites = 0;
}
void seRD () { data = 0xff; }
void seWD () { secOam[(ppudot >> 1) - 1] = data; }
void seRR ()
{
	if (ppudot == 65)
		nSprite1 = (ppuOamAddress >> 2);
	if (nSprite1 == 64) {
		oamOverflow1 = 1;
		nSprite1 = 0;
		ppuOamAddress = 0;
	}
	data = oam[(nSprite1 << 2) + nData]; /* read y coordinate */
	if (!nData && !(data <= vCounter && vCounter <= (data + 7 + ( (ppuController >> 2) & 0x08)))) /* not within range */
	{
		nSprite1++;
		ppuOamAddress += 4;
	}
	else
	{
		if (!nData  && !oamOverflow1)
		{
			foundSprites++;
			if (foundSprites > 8)
				ppuStatusOverflow = 1;
		}
		nData++;
		if (nData == 4)
		{
			nData = 0;
			nSprite1++;
			ppuOamAddress += 4;

		}
	}
}

void seWW ()
{
	if (!(oamOverflow1 | oamOverflow2))
	{
		secOam[(nSprite2 << 2) + nData2] = data;
		if (nData2 == 3)
		{
			nData2 = 0;
			nSprite2++;
			if (nSprite2 == 8)
			{
				nSprite2 = 0;
				oamOverflow2 = 1;
			}
		}
		else
		{
			if (nData2 == 1 && nSprite1 == 0)
				isSpriteZero = nSprite2;
			nData2 = nData;
		}
	}
}

void tfNT ()
{
	uint_fast16_t a = (0x2000 | (ppuV & 0xfff));
	ntData = *ppuread(a);

}
void tfAT ()
{
	uint_fast16_t a = (0x23c0 | (ppuV & 0xc00) | ((ppuV >> 4) & 0x38) | ((ppuV >> 2) & 0x07));
	bgData = *ppuread(a);
	attData = ((bgData >> ((((ppuV >> 1) & 1) | ((ppuV >> 5) & 2)) << 1)) & 3);

}
void tfLT ()
{
	uint_fast16_t a = ((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7));
	tileLow = *ppuread(a);

}
void tfHT ()
{
	uint_fast16_t a = ((ntData << 4) + ((ppuController & 0x10) << 8) + ((ppuV >> 12) & 7) + 8);
	tileHigh = *ppuread(a);
	toggle_a12(a);
}
void sfNT () { ppuOamAddress = 0; }
void sfAT () { ppuOamAddress = 0; }

static uint_fast8_t cSprite, *sprite, yOffset, flipY, spriteRow;
static uint_fast16_t patternOffset;

void sfLT ()
{
	cSprite = ((ppudot >> 3) & 0x07);
	sprite = secOam + (cSprite << 2);
	yOffset = vCounter - (sprite[0]);
	flipY = ((sprite[2] >> 7) & 1);
	spriteRow = (yOffset & 7) + flipY * (7 - ((yOffset & 7) << 1));
	if (ppuController & 0x20) /* 8x16 sprites */
		patternOffset = (((sprite[1] & 0x01) << 12) 	/* select pattern table */
		+ ((sprite[1] & 0xfe) << 4) 					/* offset for top tile (16b per tile) */
		+ (((yOffset << 1) ^ (flipY << 4)) & 0x10)); 	/* select bottom tile if either offset 8+ or flipped sprite */
	else if (!(ppuController & 0x20)) /* 8x8 sprites */
		patternOffset = (sprite[1] << 4) + ((ppuController & 0x08) ? 0x1000 : 0);
	spriteLow = *ppuread(patternOffset + spriteRow);
	ppuOamAddress = 0;
}

void sfHT ()
{
		spriteHigh = *ppuread(patternOffset + spriteRow + 8);
		ppuOamAddress = 0;
		uint_fast8_t flipX = ((sprite[2] >> 6) & 1);
		uint_fast8_t nPalette = (sprite[2] & 3);
	/* decode pattern data and store in buffer */
		for (int pcol = 0; pcol < 8; pcol++)
		{
			uint_fast8_t spritePixel = (7 - pcol - flipX * (7 - (pcol << 1)));
			uint_fast8_t pixelData = (((spriteLow >> spritePixel) & 0x01) +
						   (((spriteHigh >> spritePixel) & 0x01) << 1));
			toggle_a12(patternOffset + spriteRow);
			if (pixelData && (spriteBuffer[sprite[3] + pcol])==0xff && !(sprite[3] == 0xff))
			{/* TODO: this PPU access probably should not be here, screws up mmc3 irq? */
				spriteBuffer[sprite[3] + pcol] = *ppuread(0x3f10 + (nPalette << 2) + pixelData);
				if (isSpriteZero == cSprite)
				{
					zeroBuffer[sprite[3] + pcol] = 1;
				}
				priorityBuffer[sprite[3] + pcol] = (sprite[2] & 0x20);
			}
		}

}
void dfNT () { }
void hINC()
{
if (ppuMask & 0x18)
	{
	if ((ppuV & 0x1f) == 0x1f)
	{
		ppuV &= ~0x1f;
		ppuV ^= 0x0400; /* switch nametable horizontally */
	}
	else
		ppuV++; /* next tile */
	}
}
void vINC()
{
	if (ppuMask & 0x18)
	{
		if ((ppuV & 0x7000) != 0x7000)
			ppuV += 0x1000;
		else
		{
			ppuV &= ~0x7000;
			uint_fast8_t coarseY = ((ppuV & 0x3e0) >> 5);
			if (coarseY == 29)
			{
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

void reload_tile_shifter()
{
	tileShifterLow = ((tileShifterLow & 0xff00) | tileLow);
	tileShifterHigh = ((tileShifterHigh & 0xff00) | tileHigh);
	attShifterLow = ((attShifterLow & 0xff00) | ((attData & 1) ? 0xff : 0x00));
	attShifterHigh = ((attShifterHigh & 0xff00) | ((attData & 2) ? 0xff : 0x00));
}

void ppu_render()
{/* save for render
 * color mask
 *
 * The shifters shift at h=2, the palette address changes at h=3 for the palette lookup, and the pixel is drawn during h=4 (as seen by vid_ changing).
 * It also looks like the shift registers are actually reloaded at h=9,17,25,... instead of at h=8,16,24,
 */
	if (ppudot >= 1 && ppudot <= 257) /* tile data fetch */
	{
		if (ppudot <=256)
		{
		if (ppudot%8 == 2)
			reload_tile_shifter();
		}
		if (ppudot >=2)
		{
		uint_fast8_t pValue;
		int16_t cDot = ppudot - 2;
		pValue = ((attShifterHigh >> (12 - ppuX)) & 8) | ((attShifterLow >> (13 - ppuX)) & 4) | ((tileShifterHigh >> (14 - ppuX)) & 2) | ((tileShifterLow >> (15 - ppuX)) & 1);
		uint_fast8_t bgColor = (!(ppuMask & 0x18) && (ppuV & 0x3f00) == 0x3f00) ? *ppuread(ppuV & 0x3fff) : *ppuread(0x3f00);
		uint_fast8_t isSprite = (spriteBuffer[cDot]!=0xff && (ppuMask & 0x10) && ((cDot > 7) || (ppuMask & 0x04)));
		uint_fast8_t isBg = ((pValue & 0x03) && (ppuMask & 0x08) && ((cDot > 7) || (ppuMask & 0x02)));
		if (vCounter < 240)
		{
			if (isSprite && isBg)
			{
				if (!ppuStatusSpriteZero && zeroBuffer[cDot] && cDot<255)
				{
					ppuStatusSpriteZero = 1;
					frameBuffer[vCounter][cDot] = 0x10;
				}
				frameBuffer[vCounter][cDot] = priorityBuffer[cDot] ?  *ppuread(0x3f00 + pValue) : spriteBuffer[cDot];
			}
			else if (isSprite && !isBg)
			{
				frameBuffer[vCounter][cDot] = spriteBuffer[cDot];
			}
			else if (isBg && !isSprite)
				frameBuffer[vCounter][cDot] = *ppuread(0x3f00 + pValue);
			else
				frameBuffer[vCounter][cDot] = bgColor;
			}
			tileShifterHigh = (tileShifterHigh << 1);
			tileShifterLow = (tileShifterLow << 1);
			attShifterHigh = (attShifterHigh << 1);
			attShifterLow = (attShifterLow << 1);
		if (ppudot == 257)
		{
			nSprite2 = 0;
			memset(spriteBuffer,0xff,256);
			memset(priorityBuffer,0,256);
			memset(zeroBuffer,0,256);
		}
		}
	}
	else if (ppudot >= 321 && ppudot <= 336) /* prefetch tiles */
	{
		if (ppudot%8 == 1)
			reload_tile_shifter();
		tileShifterHigh = (tileShifterHigh << 1);
		tileShifterLow = (tileShifterLow << 1);
		attShifterHigh = (attShifterHigh << 1);
		attShifterLow = (attShifterLow << 1);
	}
}

void horizontal_t_to_v()
{
	if (ppuMask & 0x18)
	{
	ppuV = (ppuV & 0xfbe0) | (ppuT & 0x41f); /* reset x scroll */
	}
}

void vertical_t_to_v()
{
	if (ppuMask & 0x18)
	{
	ppuV = (ppuV & 0x841f) | (ppuT & 0x7be0); /* reset Y scroll */
	}
}

void draw_nametable () {
	uint_fast8_t	*tilesrcA, npalA, attsrcA, paletteIndexA, *tilesrcB, npalB, attsrcB, paletteIndexB, *tilesrcC, npalC, attsrcC, paletteIndexC, *tilesrcD, npalD, attsrcD, paletteIndexD;
	uint_fast16_t tileOffsetA, tileOffsetB, tileOffsetC, tileOffsetD;
	for (int tileRow = 0; tileRow < 30; tileRow++) {
		for (int tileColumn = 0; tileColumn < 32; tileColumn++) {
			tileOffsetA = 16 * nameSlot[0][tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcA = &chrSlot[(tileOffsetA>>10)][tileOffsetA&0x3ff];
		    attsrcA = nameSlot[0][0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalA = ((attsrcA >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);

			tileOffsetB = 16 * nameSlot[1][tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcB = &chrSlot[(tileOffsetB>>10)][tileOffsetB&0x3ff];
		    attsrcB = nameSlot[1][0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalB = ((attsrcB >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);

			tileOffsetC = 16 * nameSlot[2][tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcC = &chrSlot[(tileOffsetC>>10)][tileOffsetC&0x3ff];
		    attsrcC = nameSlot[2][0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalC = ((attsrcC >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);

			tileOffsetD = 16 * nameSlot[3][tileRow * 32 + tileColumn] + ((ppuController & 0x10) <<8);
		    tilesrcD = &chrSlot[(tileOffsetD>>10)][tileOffsetD&0x3ff];
		    attsrcD = nameSlot[3][0x3c0 + (tileRow >> 2) * 8 + (tileColumn >> 2)];
		    npalD = ((attsrcD >> (((tileColumn >> 1) & 1) | (tileRow & 2) ) * 2) & 3);

		    for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
				for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {
					paletteIndexA = (((tilesrcA[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcA[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));
					nameBuffer[(tileRow<<3) + pixelRow][(tileColumn<<3) + pixelColumn] =
									paletteIndexA ? *ppuread(0x3f00 + npalA * 4 + paletteIndexA) : *ppuread(0x3f00);
					paletteIndexB = (((tilesrcB[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcB[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));
					nameBuffer[(tileRow<<3) + pixelRow][SWIDTH + (tileColumn<<3) + pixelColumn] =
									paletteIndexB ? *ppuread(0x3f00 + npalB * 4 + paletteIndexB) : *ppuread(0x3f00);
					paletteIndexC = (((tilesrcC[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcC[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));
					nameBuffer[SHEIGHT + (tileRow<<3) + pixelRow][(tileColumn<<3) + pixelColumn] =
									paletteIndexC ? *ppuread(0x3f00 + npalC * 4 + paletteIndexC) : *ppuread(0x3f00);
					paletteIndexD = (((tilesrcD[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
										  + ( (tilesrcD[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0));
					nameBuffer[SHEIGHT + (tileRow<<3) + pixelRow][SWIDTH + (tileColumn<<3) + pixelColumn] =
									paletteIndexD ? *ppuread(0x3f00 + npalD * 4 + paletteIndexD) : *ppuread(0x3f00);
				}
			}
		}
	}
	int x = ((ppuV & 0x1f) - 2);
	int y = ((ppuV >> 5) & 0x1f);
	int nx = ((ppuV >> 10) & 0x01);
	int ny = ((ppuV >> 11) & 0x01);
	int xStart = (nx * SWIDTH + x * 8);
	int yStart = (ny * SHEIGHT + y * 8);
	int xEnd = ((nx * SWIDTH + x * 8 + SWIDTH) - 1);
	int yEnd = ((ny * SHEIGHT + y * 8 + SHEIGHT) - 1);

	for (int xy = 0; xy < 256; xy++)
	{
		int xOffset = (nx * SWIDTH + x * 8 + xy);
		int yOffset = (ny * SHEIGHT + y * 8 + xy);
		if (xy > 239)
			yOffset = (ny * SHEIGHT + y * 8);
		nameBuffer[yStart%(SHEIGHT<<1)][xOffset%(SWIDTH<<1)] = 0x30;
		nameBuffer[yOffset%(SHEIGHT<<1)][xStart%(SWIDTH<<1)] = 0x30;
		nameBuffer[yEnd%(SHEIGHT<<1)][xOffset%(SWIDTH<<1)] = 0x30;
		nameBuffer[yOffset%(SHEIGHT<<1)][xEnd%(SWIDTH<<1)] = 0x30;
	}
}

void draw_pattern () {
	uint_fast8_t	*tilesrcA, npalA;
	uint_fast16_t tileOffsetA, palSourceA;
	uint_fast8_t	*tilesrcB, npalB;
	uint_fast16_t tileOffsetB, palSourceB;
	for (int tileRow = 0; tileRow < 16; tileRow++) {
		for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
			tileOffsetA = 16 * ((tileRow << 4) + tileColumn);
		    tilesrcA = &chrSlot[(tileOffsetA>>10)][tileOffsetA&0x3ff];
		    npalA = 0;
			tileOffsetB = 16 * ((tileRow << 4) + tileColumn) + 0x1000;
		    tilesrcB = &chrSlot[(tileOffsetB>>10)][tileOffsetB&0x3ff];
		    npalB = 0;
		    palSourceA = (ppuController & 0x10) ? 0x3f10 : 0x3f00;
		    palSourceB = (ppuController & 0x08) ? 0x3f10 : 0x3f00;

			for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
				for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {

					patternBuffer[(tileRow<<3) + pixelRow][         (tileColumn<<3) + pixelColumn] =
						*ppuread(palSourceA + npalA * 4
						+ (((tilesrcA[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
						+ ( (tilesrcA[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0)));

					patternBuffer[(tileRow<<3) + pixelRow][(SWIDTH>>1) + (tileColumn<<3) + pixelColumn] =
						*ppuread(palSourceB + npalB * 4
						+ (((tilesrcB[pixelRow]     & (1<<(7-pixelColumn))) ? 1 : 0)
						+ ( (tilesrcB[pixelRow + 8] & (1<<(7-pixelColumn))) ? 2 : 0)));
				}
			}
		}
	}
}

void draw_palette () {
	for (int tileRow = 0; tileRow < 2; tileRow++) {
		for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
			for (int pixelRow = 0; pixelRow < 8; pixelRow++) {
				for (int pixelColumn = 0; pixelColumn < 8; pixelColumn++) {
					paletteBuffer[(tileRow<<3) + pixelRow][         (tileColumn<<3) + pixelColumn] =
						*ppuread(0x3f00 + (tileRow << 4) + tileColumn);
				}
			}
		}
	}
}

static uint_fast8_t ppureg = 0, vbuff = 0;
uint_fast8_t read_ppu_register(uint_fast16_t addr) {
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
		if ((ppuMask & 0x18) && !vblank_period)
		{
			if (ppudot < 65)
				tmpval8 = 0xff;
			else if (ppudot <257)
				tmpval8 = secOam[(nSprite2 << 2) + nData2];
			else if (ppudot < 321)
			{
				cSprite = ((ppudot >> 3) & 0x07);
				tmpval8 = secOam + (cSprite << 2);
			}
			else
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

void write_ppu_register(uint_fast16_t addr, uint_fast8_t tmpval8) {
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
		if (vblank_period || !(ppuMask & 0x18))
		{
			ppuData = tmpval8;
			if ((ppuV & 0x3fff) >= 0x3f00)
				ppuwrite((ppuV & 0x3fff),(ppuData & 0x3f));
			else
			{
				ppuwrite((ppuV & 0x3fff),ppuData);
			}
			ppuV += (ppuController & 0x04) ? 32 : 1;
			toggle_a12(ppuV);
			break;
		}
		else
		{
			vINC();
			hINC();
		}
	}
}

uint_fast8_t * ppuread(uint_fast16_t address)
{
	if (address < 0x2000) /* pattern tables */
	{
		return &chrSlot[(address>>10)][address&0x3ff];
	}
	else if (address >= 0x2000 && address <0x3f00) /* nametables */
		return &nameSlot[(address>>10) & 3][address&0x3ff];
	else if (address >= 0x3f00) { /* palette RAM */
		if (address == 0x3f10)
			address = 0x3f00;
		else if (address == 0x3f14)
			address = 0x3f04;
		else if (address == 0x3f18)
			address = 0x3f08;
		else if (address == 0x3f1c)
			address = 0x3f0c;
		return &palette[(address&0x1f)];
	}
	return 0;
}

void ppuwrite(uint_fast16_t address, uint_fast8_t value) {
	if (address < 0x2000) /* pattern tables */
	{
		if (chrSource[(address>>10)] == CHR_RAM)
			chrSlot[(address>>10)][address&0x3ff] = value;
	}
	else if (address >= 0x2000 && address <0x3f00) /* nametables */
		nameSlot[(address>>10) & 3][address&0x3ff] = value;
	else if (address >= 0x3f00) { /* palette RAM */
		if (address == 0x3f10)
			address = 0x3f00;
		else if (address == 0x3f14)
			address = 0x3f04;
		else if (address == 0x3f18)
			address = 0x3f08;
		else if (address == 0x3f1c)
			address = 0x3f0c;
		palette[(address&0x1f)] = value;
	}
}

void check_nmi()
{
	if ((ppuController & 0x80) && ppuStatusNmi && !nmiPulled && !nmiSuppressed)
	{
		nmiPulled = 1;
		nmiFlipFlop = ppucc;
	}
}

uint_fast16_t lastAddress = 0x0000;
void toggle_a12(uint_fast16_t address)
{
	if ((address & 0x1000) && ((address ^ lastAddress) & 0x1000) && (!strcmp(cart.slot,"txrom") || !strcmp(cart.slot,"tqrom") || !strcmp(cart.slot,"txsrom") || !strcmp(cart.slot,"tc0190fmcp")))
	{
		irq_ppu_clocked();
	}
	else if (!(address & 0x1000) && ((address ^ lastAddress) & 0x1000) && !strcmp(cart.slot,"txrom"))
	{
	}
	/* during sprite fetches, the PPU rapidly alternates between $1xxx and $2xxx, and the MMC3 does not see A13 -
	 * as such, the PPU will send 8 rising edges on A12 during the sprite fetch portion of the scanline
	 * (with 8 pixel clocks, or 2.67 CPU cycles between them */
	lastAddress = address;
}
