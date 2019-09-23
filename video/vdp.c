/* TMS9918a: the original used by SG-1000/SC-3000 (and MSX and ColecoVision)
 * 315-5124: Customized TMS9918a with Graphics Mode 4 added
 * 			 -has 16-bit VRAM address bus (vs 8-bit)
 * 			 -Extended RGB color palette - defined in CRAM
 * 			 -4bpp tiles
 * 			 -support for double the amount of sprites
 * 			 -line interrupts (for split screen effects)
 * 			 -big sprites are now 16x8 (instead of 16x16)
 * 315-5246: Revision of 315-5124 used in later SMS consoles and all(?) SMS2 consoles
 * 			 -adds extended scanline modes (used by Codemasters games)
 *			 -fixes the sprite zoom bug in 315-5124
 * 315-5377: 315-5246 with an added graphics mode (Game Gear)
 * 			 -gg mode:
 * 			 	-improved palette - 4bit rgb (gg mode)
 * 			 	-CRAM is doubled in size (gg mode)
 * 315-5313: The version used in Mega Drive
 * 			 -adds several modes specific to Mega Drive
 * 			 -removes support for modes 0-3
 * 			 -removes support for sprite zooming
 * 			 -adds mode 5: (http://gendev.spritesmind.net/forum/viewtopic.php?t=1394)
 * 			 	-improved palette (3bit rgb)
 * 			 	-64kb VRAM - different addressing
 *
 *TODO: emulate version differences:
 * -screen height
 * -masking of nametable address
 * -zooming: bug in vdp1, works in vdp2, not at all in md http://benryves.com/journal/2889425
 * -VDP controls NMI
 * Timings:
 * 	http://www.smspower.org/forums/8161-SMSDisplayTiming - charles measurements
 * 	http://www.smspower.org/forums/10695-SMSVDPTester?start=50&sid=20f9ccfe73af4559d1acb2fdb9931e25 - vdp test discussion
 */

#include "vdp.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /*exit */
#include "../sms/smsemu.h"
#include "../cpu/z80.h"
#include "../my_sdl.h"
#include "../jemu.h"

uint16_t lineCounter, lineReload;
uint8_t controlFlag = 0, statusFlags = 0, readBuffer = 0, bgColor = 0, textColor, bgXScroll, bgYScroll, lineInt = 0;
uint8_t vScrollLock, hScrollLock, columnMask, lineInterrupt, spriteShift, externalSync, displayEnable, frameInterrupt, spriteSize, spriteZoom, videoMode;
int sframe = 0;

/* REGISTERS */
uint8_t modeControl1, modeControl2, codeReg;
struct vdpDisplayMode ntsc192={256,342,240,262,192,216,219,222,235,262,224}, *vdpCurrentMode;
struct vdpDisplayMode ntsc224={256,342,240,262,224,232,235,238,251,262,256};
struct vdpDisplayMode pal192={256,342,288,313,192,240,243,246,259,313,224};
struct vdpDisplayMode pal224={256,342,288,313,224,256,259,262,275,313,256};
int16_t vdpdot;
uint16_t controlWord, vCounter = 0, hCounter = 0, addReg, ntAddress, ntMask, sgAddress, saAddress, ctAddress, pgAddress, pgMask;
/* Mapped memory */					/* TODO: dynamically allocate screenBuffer */
uint32_t *vdpScreenBuffer;
uint8_t vram[VRAM_SIZE], cram[CRAM_SIZE], smsColor[0xc0],
colorTable[0x30] = {  0,   0,   0,   0,   0,   0,  33, 200,  66,  94, 220, 120,
					 84,  85, 237, 125, 118, 252, 212,  82,  77,  66, 235, 245,
					252,  85,  84, 255, 121, 120, 212, 193,  84, 230, 206, 128,
					 33, 176,  59, 201,  91, 186, 204, 204, 204, 255, 255, 255 };
uint8_t *currentClut;
static inline void render_scanline(void), set_video_mode(void);

void init_vdp(){
	set_video_mode();
	for (int i = 0; i < 0x40; i++) {
		smsColor[i*3]     = (((i & 0x03) << 6) | ((i & 0x03) << 4) | ((i & 0x03) << 2) | (i & 0x03));
		smsColor[(i*3)+1] = (((i & 0x0c) << 4) | ((i & 0x0c) << 2) | ((i & 0x0c) >> 2) | (i & 0x0c));
		smsColor[(i*3)+2] = (((i & 0x30) << 2) | ((i & 0x30) >> 4) | ((i & 0x30) >> 2) | (i & 0x30));
	}
	pgAddress = 0;
	vdpdot = -94;
	if(vdpScreenBuffer)
		free(vdpScreenBuffer);
	vdpScreenBuffer = (uint32_t*)malloc(vdpCurrentMode->height * vdpCurrentMode->width * sizeof(uint32_t));
	memset(vdpScreenBuffer, 0xff0000ff, vdpCurrentMode->height * vdpCurrentMode->width * sizeof(uint32_t));

	/* TODO: can this be simplfied by thinking of v counter as signed 8 bit? */
	if(!ntsc192.vcount){
		ntsc192.vcount = (uint8_t*) malloc(ntsc192.fullheight * sizeof(uint8_t));
		for(int i=0;i<0xdb;i++){
			ntsc192.vcount[i] = i;
		}
		for(int i=0xd5;i<0x100;i++){
			ntsc192.vcount[i+6] = i;
		}
	}
	if(!ntsc224.vcount){
		ntsc224.vcount = (uint8_t*) malloc(ntsc224.fullheight * sizeof(uint8_t));
		for(int i=0;i<0xeb;i++){
			ntsc224.vcount[i] = i;
		}
		for(int i=0xe5;i<0x100;i++){
			ntsc224.vcount[i+6] = i;
		}
	}
	if(!pal192.vcount){
		pal192.vcount = (uint8_t*) malloc(pal192.fullheight * sizeof(uint8_t));
		for(int i=0;i<0xf3;i++){
			pal192.vcount[i] = i;
		}
		for(int i=0xba;i<0x100;i++){
			pal192.vcount[i+39] = i;
		}
	}
	if(!pal224.vcount){
		pal224.vcount = (uint8_t*) malloc(pal224.fullheight * sizeof(uint8_t));
		for(int i=0;i<0x103;i++){
			pal224.vcount[i] = i;
		}
		for(int i=0xca;i<0x100;i++){
			pal224.vcount[i+39] = i;
		}
	}
}

void close_vdp(){
	free (vdpScreenBuffer);
	free (ntsc192.vcount);
	free (ntsc224.vcount);
	free (pal192.vcount);
	free (pal224.vcount);
}

void default_video_mode(){
	if(currentMachine->videoSystem == NTSC)
		vdpCurrentMode = &ntsc192;
	else if(currentMachine->videoSystem == PAL)
		vdpCurrentMode = &pal192;
}

void set_video_mode(){
	/* Mode 5 is probably (mode2 & 0x04) */
	videoMode = (((modeControl1 & 0x04) << 1) | ((modeControl2 & 0x08) >> 1) | (modeControl1 & 0x02) | ((modeControl2 & 0x10) >> 4));
	if(videoMode == 0x9 || videoMode == 0xd) /* text mode TODO: valid for sms2 only */
		videoMode = 1;
	else if(videoMode == 0xb && currentMachine->videoCard >= VDP_2){
		if(currentMachine->videoSystem == NTSC)
			vdpCurrentMode = &ntsc224;
		else if(currentMachine->videoSystem == PAL)
			vdpCurrentMode = &pal224;
	}
	else if(videoMode == 0xe  && currentMachine->videoCard >= VDP_2){
		printf("Sets 240 line mode\n");
	}
	else{
		default_video_mode();
	}
	if(videoMode & 0x08)
		currentClut = smsColor;
	else
		currentClut = colorTable;
}

void write_vdp_control(uint8_t value){
controlWord = controlFlag ? ((controlWord & 0x00ff) | (value << 8)) : ((controlWord & 0xff00) | value);
controlFlag ^= 1;
addReg = (controlWord & VRAM_MASK);
codeReg = (controlWord >> 14);
if (!controlFlag){
	switch (codeReg){
	case 0:
		read_vdp_data();
		break;
	case 2: /* VDP register write */
		switch (controlWord & 0x0f00){
		case 0x0000: /* Mode Control No. 1 */
			/* bit 7 sets external vdp */
			modeControl1 = (controlWord & 0xff);
			if((modeControl1 & 0x01))
				printf("External video enabled\n");
			vScrollLock = (modeControl1 & 0x80); 		/* mode 4 only */
			hScrollLock = (modeControl1 & 0x40); 		/* mode 4 only */
			columnMask = ((modeControl1 & 0x20) >> 2);	/* mode 4 only */
			lineInterrupt = (modeControl1 & 0x10);		/* mode 4 only */
			spriteShift = (modeControl1 & 0x08);		/* mode 4 only */
			externalSync = (modeControl1 & 0x01); 		/* external video */
			set_video_mode();
			break;
		case 0x0100: /* Mode Control No. 2 */
			modeControl2 = (controlWord & 0xff);
			/*if(controlWord & 0x80)						 RAM size selection */
			displayEnable = (controlWord & 0x40);
			frameInterrupt = (controlWord & 0x20);
			spriteSize = (controlWord & 0x02);			/* dependent on video mode */
			spriteZoom = (controlWord & 0x01);			/* buggy in 315-5124 only */
			set_video_mode();
			break;
		case 0x0200: /* Name Table Base Address */
			ntMask = ((controlWord << 10) & NT_MASK); /* TODO: set to 0xf in newer vdp versions */
			ntAddress = (ntMask & 0x3800);
			break;
		case 0x0300: /* Color Table Base Address */
			if(videoMode == 2)
				ctAddress = ((controlWord & 0x80) << 6);
			else
				ctAddress = ((controlWord & 0xff) << 6); /* 0x3fc0 */
			break;
		case 0x0400: /* Background Pattern Generator Base Address */
			pgMask = ((controlWord << 11) & PG_MASK);
			if(videoMode & 0x02)
				pgAddress = (pgMask & 0x2000);
			else
				pgAddress = pgMask; /* 0x3800 */
			break;
		case 0x0500: /* Sprite Attribute Table Base Address */
			saAddress = ((controlWord & 0x7e) << 7); /* TODO: mask should depend on mode */
			break;
		case 0x0600: /* Sprite Pattern Generator Base Address */
			if(videoMode & 0x08)
				sgAddress = ((controlWord & 0x04) << 11); /* TODO: mask should depend on mode */
			else
				sgAddress = ((controlWord & 0x07) << 11);
			break;
		case 0x0700: /* Overscan/Backdrop Color */
			textColor = ((controlWord & 0xf0) >> 4);
			bgColor = (controlWord & 0x0f); /* TODO: upper 4 bits are used in text mode */
			break;
		case 0x0800: /* Background X Scroll */
			bgXScroll = (controlWord & 0xff); /* mode 4 only */
			break;
		case 0x0900: /* Background Y Scroll */
			bgYScroll = (controlWord & 0xff); /* mode 4 only */
			break;
		case 0x0a00: /* Line counter */
			lineReload = (controlWord & 0xff);/* mode 4 only */
			break;
		default:
			printf("Write to undefined VDP register\n");
			break;
		}
		break;
	}
}
}

void write_vdp_data(uint8_t value){
	/* addressing works differently in 4k mode */
	if(codeReg < 3)
		vram[addReg++ & VRAM_MASK] = value;
	else if(codeReg == 3)
		cram[addReg++ & CRAM_MASK] = value;
	readBuffer = value;
	controlFlag = 0;
}

uint8_t read_vdp_data(){
	uint8_t value = readBuffer;
	readBuffer = vram[addReg++ & 0x3fff];
	controlFlag = 0;
	return value;
}

void run_vdp(int cycles){
while (cycles) {
	if(vdpdot == 590){//HCOUNT jumps in the middle of HBLANK
		vdpdot = -94;
		if(vCounter < vdpCurrentMode->height)
			render_scanline();

	}
	if(vdpdot == -48)
		vCounter++;
	if(vCounter == vdpCurrentMode->fullheight){
		sframe++;
		vCounter = 0;
		z80_irqPulled = 0;
		render_frame(vdpScreenBuffer);
	}
	else if ((vCounter == vdpCurrentMode->vactive) && (vdpdot == -52)){
		statusFlags |= INT;
	}
	if ((vCounter <= vdpCurrentMode->vactive) && (vdpdot == -51)){
		lineCounter--;
		if ((lineCounter & 0xff) == 0xff){
			lineCounter = lineReload;
			lineInt = 1;
		}
	}
	else if ((vCounter > vdpCurrentMode->vactive) && (vdpdot == -51))
		lineCounter = lineReload;
	if (( ((statusFlags & INT) && frameInterrupt) || ((lineInt) && lineInterrupt) ) && !z80_irqPulled)
		z80_irqPulled = 1;
	else if ((!frameInterrupt /*|| (!(mode1 & 0x10))*/) && z80_irqPulled)/* TODO: disabling line interrupt should deassert the IRQ line */
			z80_irqPulled = 0;
	cycles--;
	vdpdot++;
}
}
uint8_t blank=0x15, black=0x00;
void render_scanline(){
	uint8_t pixel, tileRow, ntColumn, tileColumn, ntRow, spriteX, spriteI, spriteBuffer = 0, spriteMask[vdpCurrentMode->width], priorityMask[vdpCurrentMode->width], transMask[vdpCurrentMode->width], color, cidx;
	int16_t spriteY;
	uint16_t ntData, pgOffset, ctOffset, sgOffset, ntOffset;
	memset(spriteMask, 0, vdpCurrentMode->width*sizeof(uint8_t));
	memset(priorityMask, 0, vdpCurrentMode->width*sizeof(uint8_t));
	memset(transMask, 0, vdpCurrentMode->width*sizeof(uint8_t));
	uint16_t yOffset = vCounter + (vdpCurrentMode->tborder - vdpCurrentMode->tblank);

	if ((vCounter < vdpCurrentMode->vactive) && displayEnable){ /* during active display */
		uint16_t scroll = (hScrollLock && vCounter < 16) ? 0 : bgXScroll;

		for (uint8_t screenColumn = 0; screenColumn < 32; screenColumn++){
			ntRow = ((vCounter + ((vScrollLock && screenColumn >= 24) ? 0 : bgYScroll)) % vdpCurrentMode->vwrap);
			ntColumn = 32 - ((scroll & 0xf8) >> 3) + screenColumn;

			/* Find the address of the current tile name */
			if(videoMode & 0x08){
				if(vdpCurrentMode->vactive == 192)
					ntOffset = ((ntAddress + ((ntRow & 0xf8) << 3) + ((ntColumn & 0x1f) << 1)) & (ntMask | 0x3ff));
				else if(vdpCurrentMode->vactive >= 224) /* only on later vdp revisions */
					ntOffset = ((ntAddress & 0x3000) + 0x700 + ((ntRow & 0xf8) << 3) 							    + ((ntColumn & 0x1f) << 1));
			}
			else if(videoMode & 0x02) /* Graphics II mode */
				ntOffset = (ntAddress + ((ntRow & 0xf8) << 2) + (ntColumn & 0x1f));

			/* Fetch current tile name */
			ntData = vram[ntOffset];
			if(videoMode & 0x08)
				ntData |= (vram[ntOffset + 1] << 8);

			/* Find the address of the corresponding pattern generator */
			if(videoMode == 2)
				pgOffset = (pgAddress + (((vCounter & 0xc0) << 5) & pgMask) + ((ntData & 0xff) << 3));
			else if(videoMode & 0x08)
				pgOffset = ((ntData & 0x1ff) << 5);

			/* Find the address of the corresponding color (not mode 4) */
			if(videoMode == 2)
				ctOffset = (ctAddress + ((vCounter & 0xc0) << 5) + ((ntData & 0xff) << 3));

			/* Generate the current row of the pattern */
			tileRow = ((ntData & 0x400) && (videoMode & 0x08)) ? 7-(ntRow & 7) : (ntRow & 7);
			int targetPixel;
			for (uint8_t pixelIndex = 0; pixelIndex < 8; pixelIndex++){
				tileColumn = ((ntData & 0x200) && (videoMode & 0x08)) ? pixelIndex : 7-pixelIndex;
				pixel  = (vram[pgOffset + (tileRow << ((videoMode & 0x08) ? 2 : 0))] & (1 << tileColumn)) ? 1:0;
				if(videoMode & 0x08){ /* 4bpp */
					pixel |= (vram[pgOffset + (tileRow << 2) + 1] & (1 << tileColumn)) ? 2:0;
					pixel |= (vram[pgOffset + (tileRow << 2) + 2] & (1 << tileColumn)) ? 4:0;
					pixel |= (vram[pgOffset + (tileRow << 2) + 3] & (1 << tileColumn)) ? 8:0;
				}

				/* Output the pattern to the screen buffer */
				if(videoMode == 2){
					targetPixel = (((screenColumn << 3) + pixelIndex) & 0xff);
					color = (vram[ctOffset + tileRow] ? vram[ctOffset + tileRow] : bgColor);
					cidx = ((pixel ? (color >> 4) : (color & 0xf)) * 3);
				}
				else{
					targetPixel = (((screenColumn << 3) + pixelIndex + (scroll & 7)) & 0xff);
					cidx = ((cram[pixel + ((ntData & 0x800) ? 0x10 : 0)] & 0x3f) * 3);
				}
				vdpScreenBuffer[(yOffset * vdpCurrentMode->width) + targetPixel] = (0xff000000 | (currentClut[cidx] << 16) | (currentClut[cidx + 1] << 8) | currentClut[cidx + 2]);
				priorityMask[targetPixel] = ((ntData & 0x1000) >> 8);
				transMask[targetPixel] = pixel ? 1 : 0;
				if(columnMask && (videoMode & 0x08)){
					cidx = ((cram[bgColor + 0x10] & 0x3f) * 3);
					vdpScreenBuffer[(yOffset*vdpCurrentMode->width) + ((pixelIndex+scroll) & 7)] = (0xff000000 | (currentClut[cidx] << 16) | (currentClut[cidx + 1] << 8) | currentClut[cidx + 2]);
				}
			}
		}

		/* Sprite rendering */
		int listSize = ((videoMode & 0x08) ? 64 : 32);
		int spriteLimit = ((videoMode & 0x08) ? 8 : 4);
		uint8_t spriteHeight = ((spriteSize ? 16 : 8) << spriteZoom);
		uint8_t spriteWidth = (((spriteSize && !(videoMode & 0x08)) ? 16 : 8) << spriteZoom);
		/* Parse the Sprite Attribute Table, looking for sprites on the current scanline */
		for(uint8_t s = 0; s < listSize; s++){
			spriteY = (vram[saAddress + ((videoMode & 0x08)? s : s << 2)] + 1);
			if((spriteY == (0xd0 + 1)) && (vdpCurrentMode->vactive == 192))
				break;
			if(spriteY >= (256 - spriteHeight + 1))
				spriteY = (0 - (256 - spriteY)); /* negative Y offset (sprites go offscreen from top) */

			/* Decode the attributes for the sprite, unless sprite limit has been reached */
			if ((vCounter >= spriteY) && (vCounter < (spriteY + spriteHeight))){
				spriteBuffer++;
				if (spriteBuffer > spriteLimit)
					statusFlags |= OVR;
				else{
					if(videoMode & 0x08){
						spriteX = vram[saAddress + (s << 1) + 128];
						spriteI = vram[saAddress + (s << 1) + 129];
						sgOffset = sgAddress + ((spriteSize ? (spriteI & 0xfe) : spriteI) << 5);
					}
					else{
						spriteX = vram[saAddress + (s << 2) + 1];
						spriteI = vram[saAddress + (s << 2) + 2];
						sgOffset = sgAddress + ((spriteSize ? (spriteI & 0xfc) : spriteI) << 3);
						spriteShift = ((vram[saAddress + (s << 2) + 3] & 0x80) >> 4);
					}

					/* Generate the current row of the sprite */
					for (uint8_t pixelIndex = 0; pixelIndex < (spriteWidth << spriteZoom); pixelIndex++){
						/* TODO: split for different video modes to keep readability */
						if(videoMode & 0x08){
							pixel  = (vram[sgOffset + (((vCounter - spriteY) >> spriteZoom) << 2)    ] & (1 << (7 - (pixelIndex >> spriteZoom)))) ? 1:0;
							pixel |= (vram[sgOffset + (((vCounter - spriteY) >> spriteZoom) << 2) + 1] & (1 << (7 - (pixelIndex >> spriteZoom)))) ? 2:0;
							pixel |= (vram[sgOffset + (((vCounter - spriteY) >> spriteZoom) << 2) + 2] & (1 << (7 - (pixelIndex >> spriteZoom)))) ? 4:0;
							pixel |= (vram[sgOffset + (((vCounter - spriteY) >> spriteZoom) << 2) + 3] & (1 << (7 - (pixelIndex >> spriteZoom)))) ? 8:0;
							cidx = (cram[pixel+0x10] & 0x3f);
						}
						else{
							pixel  = (vram[sgOffset + ((((pixelIndex & 0x08) << 1) + (vCounter - spriteY)) >> spriteZoom)] & (1 << (7 - ((pixelIndex & 7) >> spriteZoom)))) ? 1:0;
							cidx = ((vram[saAddress + (s << 2) + 3] & 0xf) ? (vram[saAddress + (s << 2) + 3] & 0xf) : bgColor);
						}
						int pixelOffset = (pixelIndex + spriteX - spriteShift); /* must be signed int */

						if(pixel && pixelOffset < vdpCurrentMode->width && pixelOffset >= columnMask){
							if (spriteMask[pixelOffset])
								statusFlags |= COL; /* set sprite collision flag */
							else{
								if((!priorityMask[pixelOffset]) || (!transMask[pixelOffset])){
									vdpScreenBuffer[(yOffset*vdpCurrentMode->width) + pixelOffset] = (0xff000000|(currentClut[cidx * 3]<<16)|(currentClut[cidx * 3 + 1]<<8)|currentClut[cidx * 3 + 2]);
								}
								spriteMask[pixelOffset]= pixel ? 1 : 0;
							}
						}
					}
				}
			}
		}
	}
	else{
		uint8_t fillValue;
		if(vCounter < (vdpCurrentMode->bborder))
			fillValue = (cram[bgColor + 0x10] & 0x3f);
		/*else if(vCounter < (currentMode->bblank))
			fillValue = blank;
		else if(vCounter < (currentMode->vblank))
			fillValue = black;
		else if(vCounter < (currentMode->tblank))
			fillValue = blank;*/
		else if(vCounter < (vdpCurrentMode->tborder))
			fillValue = (cram[bgColor + 0x10] & 0x3f);
		else
			fillValue = 0;
		for (uint16_t p = 0; p<256; p++){
			vdpScreenBuffer[(((yOffset) % vdpCurrentMode->height)*vdpCurrentMode->width) + p]
						 = (0xff000000|(currentClut[fillValue * 3]<<16)|(currentClut[fillValue * 3 + 1]<<8)|currentClut[fillValue * 3 + 2]);
		}
	}
}

void latch_hcounter(uint8_t value){
	if(value){
		hCounter = (vdpdot >> 2);
	}
}
