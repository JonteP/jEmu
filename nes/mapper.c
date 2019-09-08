#include "mapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "globals.h"
#include "6502.h"
#include "ppu.h"
#include "cartridge.h"

uint_fast8_t mapperInt = 0, expSound = 0,
			 prgBank[8], chrBank[8];
chrtype_t chrSource[0x8];
static inline void nametable_mirroring(uint_fast8_t), null_irq(), write_null(uint_fast16_t, uint_fast8_t);
static inline uint_fast8_t read_null(uint_fast16_t);

/*-----------------------------------NINTENDO------------------------------------*/

/*/////////////////////////////////*/
/*               CNROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -bus conflicts
 *
 * */
static inline void mapper_cnrom(uint_fast16_t, uint_fast8_t);

void mapper_cnrom (uint_fast16_t address, uint_fast8_t value) {
	chrBank[0] = (value << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
}

/*/////////////////////////////////*/
/*               GxROM             */
/*/////////////////////////////////*/

static inline void mapper_gxrom(uint_fast16_t, uint_fast8_t);

void mapper_gxrom (uint_fast16_t address, uint_fast8_t value)
{
	prgBank[0] = (((value >> 4) & 0x03) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();

	chrBank[0] = ((value & 0x03) << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
}

/*/////////////////////////////////*/
/*               UxROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -implement bus conflict (test roms available)
 * -should mappers 71 and 232 go here?
 *
 */

static inline void mapper_uxrom(uint_fast16_t, uint_fast8_t);
void mapper_uxrom (uint_fast16_t address, uint_fast8_t value) {
	if (!strcmp(cart.slot,"un1rom"))
		value = ((value >> 2) & 0x07);
	if (!strcmp(cart.slot,"unrom_cc")) { /* switch 0xc000 */
		prgBank[0] = 0;
		prgBank[1] = 1;
		prgBank[2] = 2;
		prgBank[3] = 3;
		prgBank[4] = (value << 2);
		prgBank[5] = prgBank[4] + 1;
		prgBank[6] = prgBank[4] + 2;
		prgBank[7] = prgBank[4] + 3;
	} else {							 /* switch 0x8000 */
		prgBank[0] = (value << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prgBank[4] = cart.pSlots - 4;
		prgBank[5] = cart.pSlots - 3;
		prgBank[6] = cart.pSlots - 2;
		prgBank[7] = cart.pSlots - 1;
	}
	prg_bank_switch();
}

/*/////////////////////////////////*/
/*              MMC 1              */
/*              SxROM              */
/*/////////////////////////////////*/

/* TODO:
 *
 * -PRG RAM banking
 */

/* mmc1 globals */
static uint_fast8_t mmc1Shift = 0, mmc1Buffer = 0, mmc1Reg0 = 0x0f, mmc1Reg1 = 0, mmc1Reg2 = 0, mmc1Reg3 = 0, mmc1PrgOffset = 0;
static inline void mapper_mmc1(uint_fast16_t, uint_fast8_t), mmc1_prg_bank_switch(), mmc1_chr_bank_switch();

void mapper_mmc1(uint_fast16_t address, uint_fast8_t value) {
	/* TODO: clean implementation - mmc1 checks write cycle instead */
	if (!dummywrite) {
	if (value & 0x80) {
		mmc1Shift = 0;
		mmc1Buffer = 0;
		mmc1Reg0 |= 0x0c;
	} else {
		mmc1Buffer = (mmc1Buffer & ~(1<<mmc1Shift)) | ((value & 1)<<mmc1Shift);
		if (mmc1Shift == 4) {
			switch ((address>>13) & 3) {
			case 0: /* Control register */
				mmc1Reg0 = mmc1Buffer;
				switch (mmc1Reg0&3) {
				case 0:
					cart.mirroring = 2;
					break;
				case 1:
					cart.mirroring = 3;
					break;
				case 2:
					cart.mirroring = 1;
					break;
				case 3:
					cart.mirroring = 0;
					break;
				}
				nametable_mirroring(cart.mirroring);
				mmc1_prg_bank_switch();
				mmc1_chr_bank_switch();
				break;
			case 1: /* CHR ROM low bank */
				mmc1Reg1 = mmc1Buffer;
				if (cart.prgSize == 0x80000) {
					mmc1PrgOffset = ((mmc1Reg1&0x10) << 2);
				}
				mmc1_chr_bank_switch();
				mmc1_prg_bank_switch();
				break;
			case 2: /* CHR ROM high bank (4k mode) */
				mmc1Reg2 = mmc1Buffer;
				if (cart.prgSize == 0x80000) {
					mmc1PrgOffset = ((mmc1Reg1&0x10) << 2);
				}
				mmc1_chr_bank_switch();
				mmc1_prg_bank_switch();
				break;
			case 3: /* PRG ROM bank */
				mmc1Reg3 = mmc1Buffer;
				if (strcmp(cart.mmc1_type,"MMC1A"))
					wramEnable = !((mmc1Reg3 >> 4) & 1);
				mmc1_prg_bank_switch();
				break;
		}
			mmc1Shift = 0;
			mmc1Buffer = 0;
		} else
			mmc1Shift++;
	}
	}
}

void mmc1_prg_bank_switch() {
	 uint_fast8_t mmc1PrgSize = (mmc1Reg0 & 0x08); /* 0 32k, 1 16k */
	 uint_fast8_t mmc1PrgSelect = (mmc1Reg0 & 0x04); /* 0 low, 1 high */
	 uint_fast8_t nSlots = cart.pSlots;
	 if (cart.prgSize == 0x80000)
		 nSlots = nSlots >> 1;
	if (mmc1PrgSize) {
		if (mmc1PrgSelect) { /* switch 0x8000, fix 0xc000 to last bank */
			prgBank[0] = (mmc1Reg3 << 2) + mmc1PrgOffset;
			prgBank[1] = prgBank[0] + 1;
			prgBank[2] = prgBank[0] + 2;
			prgBank[3] = prgBank[0] + 3;
			prgBank[4] = nSlots - 4 + mmc1PrgOffset;
			prgBank[5] = prgBank[4] + 1;
			prgBank[6] = prgBank[4] + 2;
			prgBank[7] = prgBank[4] + 3;
		} else if (!mmc1PrgSelect) { /* switch 0xc000, fix 0x8000 to first bank */
			prgBank[0] = mmc1PrgOffset;
			prgBank[1] = prgBank[0] + 1;
			prgBank[2] = prgBank[0] + 2;
			prgBank[3] = prgBank[0] + 3;
			prgBank[4] = (mmc1Reg3 << 2) + mmc1PrgOffset;
			prgBank[5] = prgBank[4] + 1;
			prgBank[6] = prgBank[4] + 2;
			prgBank[7] = prgBank[4] + 3;
		}
	}
	else if (!mmc1PrgSize) {
		prgBank[0] = (mmc1Reg3 << 3) + mmc1PrgOffset;
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prgBank[4] = prgBank[0] + 4;
		prgBank[5] = prgBank[0] + 5;
		prgBank[6] = prgBank[0] + 6;
		prgBank[7] = prgBank[0] + 7;
	}
	prg_bank_switch();
}

void mmc1_chr_bank_switch() {
	if (cart.chrSize)
	{
	uint_fast8_t mmc1ChrSize = (mmc1Reg0 & 0x10); /* 0 8k,  1 4k */
	if (mmc1ChrSize) { /* 4k banks */
		chrBank[0] = (mmc1Reg1 << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chrBank[4] = (mmc1Reg2 << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
	} else if (!mmc1ChrSize) { /* 8k bank */
		chrBank[0] = (mmc1Reg1 << 3);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chrBank[4] = chrBank[0] + 4;
		chrBank[5] = chrBank[0] + 5;
		chrBank[6] = chrBank[0] + 6;
		chrBank[7] = chrBank[0] + 7;
	}

	chr_bank_switch();
	}
}

/*/////////////////////////////////*/
/*              MMC 3              */
/*              TxROM              */
/*/////////////////////////////////*/

/* TODO:
 *
 * Support for mapper 47
 *
 *-Clean IRQ implementation
 * remaining IRQ issues:
 * -Ninja ryuukenden 2 - cut scenes
 * -Rockman3 - possible obe-line glitch in weapons screen
 * -Rockman5 - slight glitch in gyromans elevators
 */

static uint_fast8_t mmc3BankSelect = 0, mmc3Reg[0x08] = { 0 }, mmc3IrqEnable = 0,
		mmc3PramProtect, mmc3IrqLatch = 0, mmc3IrqReload = 0, mmc3IrqCounter = 0;
static chrtype_t mmc3ChrSource[0x8];
static inline void mapper_mmc3(uint_fast16_t, uint_fast8_t), mmc3_prg_bank_switch(), mmc3_chr_bank_switch();

void mapper_mmc3 (uint_fast16_t address, uint_fast8_t value) {
	switch ((address>>13) & 3) {
		case 0:
			if (!(address %2)) { /* Bank select (0x8000) */
				mmc3BankSelect = value;
				mmc3_chr_bank_switch();
				mmc3_prg_bank_switch();
			} else if (address %2) { /* Bank data (0x8001) */
				int bank = (mmc3BankSelect & 0x07);
				mmc3Reg[bank] = value;
				if (bank < 6)
				{
					if (!strcmp(cart.slot,"tqrom") && (value & 0x40))
					{
						mmc3ChrSource[bank] = CHR_RAM;
					}
					else if (!strcmp(cart.slot,"tqrom") && !(value & 0x40))
					{
						mmc3ChrSource[bank] = CHR_ROM;
					}

					mmc3_chr_bank_switch();
				}
				else
					mmc3_prg_bank_switch();
			}
			break;
		case 1:
			if (!(address %2) && strcmp(cart.slot,"txsrom")) { /* Mirroring (0xA000) */
				cart.mirroring = 1-(value & 0x01);
				nametable_mirroring(cart.mirroring);
			} else if (address%2) { /* PRG RAM protect (0xA001) */
				mmc3PramProtect = value;
			}
			break;
		case 2:
			if (!(address%2)) { /* IRQ latch (0xC000) */
				mmc3IrqLatch = value;
			} else if (address%2) { /* IRQ reload (0xC001) */
				mmc3IrqReload = 1;
				mmc3IrqCounter = 0;
			}
			break;
		case 3:
			if (!(address%2)) { /* IRQ disable and acknowledge (0xE000) */
				mmc3IrqEnable = 0;
				mapperInt = 0;
			} else if (address%2) { /* IRQ enable (0xE001) */
				mmc3IrqEnable = 1;
			}
			break;
	}
}

void mmc3_prg_bank_switch() {
	if (mmc3BankSelect & 0x40) {
		prgBank[0] = cart.pSlots - 4;
		prgBank[1] = cart.pSlots - 3;
		prgBank[4] = (mmc3Reg[6] << 1);
		prgBank[5] = prgBank[4] + 1;
	} else if (!(mmc3BankSelect & 0x40)) {
		prgBank[0] = (mmc3Reg[6] << 1);
		prgBank[1] = prgBank[0] + 1;
		prgBank[4] = cart.pSlots - 4;
		prgBank[5] = cart.pSlots - 3;
	}
	prgBank[2] = (mmc3Reg[7] << 1);
	prgBank[3] = prgBank[2] + 1;
	prg_bank_switch();
}

void mmc3_chr_bank_switch()
{
	if (mmc3BankSelect & 0x80)
	{
		if (!strcmp(cart.slot,"txsrom"))
		{
			nameSlot[0] = (mmc3Reg[2] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[1] = (mmc3Reg[3] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[2] = (mmc3Reg[4] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[3] = (mmc3Reg[5] & 0x80) ? ciram : (ciram + 0x400);
		}
		chrBank[0] = mmc3Reg[2];
		chrBank[1] = mmc3Reg[3];
		chrBank[2] = mmc3Reg[4];
		chrBank[3] = mmc3Reg[5];
		chrBank[4] = (mmc3Reg[0] & 0xfe);
		chrBank[5] = (mmc3Reg[0] | 0x01);
		chrBank[6] = (mmc3Reg[1] & 0xfe);
		chrBank[7] = (mmc3Reg[1] | 0x01);
		chrSource[0] = mmc3ChrSource[2];
		chrSource[1] = mmc3ChrSource[3];
		chrSource[2] = mmc3ChrSource[4];
		chrSource[3] = mmc3ChrSource[5];
		chrSource[4] = mmc3ChrSource[0];
		chrSource[5] = mmc3ChrSource[0];
		chrSource[6] = mmc3ChrSource[1];
		chrSource[7] = mmc3ChrSource[1];
	}
	else if (!(mmc3BankSelect & 0x80))
	{
		if (!strcmp(cart.slot,"txsrom"))
		{
			nameSlot[0] = (mmc3Reg[0] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[1] = (mmc3Reg[0] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[2] = (mmc3Reg[1] & 0x80) ? ciram : (ciram + 0x400);
			nameSlot[3] = (mmc3Reg[1] & 0x80) ? ciram : (ciram + 0x400);
		}
		chrBank[0] = (mmc3Reg[0] & 0xfe);
		chrBank[1] = (mmc3Reg[0] | 0x01);
		chrBank[2] = (mmc3Reg[1] & 0xfe);
		chrBank[3] = (mmc3Reg[1] | 0x01);
		chrBank[4] = mmc3Reg[2];
		chrBank[5] = mmc3Reg[3];
		chrBank[6] = mmc3Reg[4];
		chrBank[7] = mmc3Reg[5];
		chrSource[0] = mmc3ChrSource[0];
		chrSource[1] = mmc3ChrSource[0];
		chrSource[2] = mmc3ChrSource[1];
		chrSource[3] = mmc3ChrSource[1];
		chrSource[4] = mmc3ChrSource[2];
		chrSource[5] = mmc3ChrSource[3];
		chrSource[6] = mmc3ChrSource[4];
		chrSource[7] = mmc3ChrSource[5];
	}
	chr_bank_switch();
}

void mmc3_irq ()
{
	if (mmc3IrqReload || !mmc3IrqCounter)
	{
		mmc3IrqReload = 0;
		mmc3IrqCounter = mmc3IrqLatch;
		if (mmc3IrqEnable && !mmc3IrqCounter)
		{
			mapperInt = 1;
		}
	}
	else if (mmc3IrqCounter > 0)
	{
		mmc3IrqCounter--;
		if (mmc3IrqEnable && !mmc3IrqCounter)
		{
			mapperInt = 1;
		}
	}
}

/*-------------------------American Video Entertainment-------------------------*/

/*/////////////////////////////////*/
/*            NINA-001             */
/*/////////////////////////////////*/

static inline void mapper_nina1(uint_fast16_t, uint_fast8_t);

void mapper_nina1(uint_fast16_t address, uint_fast8_t value)
{
	switch (address)
	{
	case 0x7ffd:
		prgBank[0] = ((value & 0x01) << 3);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prgBank[4] = prgBank[0] + 4;
		prgBank[5] = prgBank[0] + 5;
		prgBank[6] = prgBank[0] + 6;
		prgBank[7] = prgBank[0] + 7;
		prg_bank_switch();
		break;
	case 0x7ffe:
		chrBank[0] = ((value & 0x0f) << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chr_bank_switch();
		break;
	case 0x7fff:
		chrBank[4] = ((value & 0x0f) << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
		chr_bank_switch();
		break;
	}
}

/*/////////////////////////////////*/
/*            NINA-003             */
/*            NINA-006             */
/*/////////////////////////////////*/

static inline void mapper_nina36(uint_fast16_t, uint_fast8_t);

void mapper_nina36(uint_fast16_t address, uint_fast8_t value)
{
	if (address & 0x4100)
	{
		prgBank[0] = (value & 0x08);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prgBank[4] = prgBank[0] + 4;
		prgBank[5] = prgBank[0] + 5;
		prgBank[6] = prgBank[0] + 6;
		prgBank[7] = prgBank[0] + 7;
		prg_bank_switch();
		chrBank[0] = ((value & 0x07) << 3);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chrBank[4] = chrBank[0] + 4;
		chrBank[5] = chrBank[0] + 5;
		chrBank[6] = chrBank[0] + 6;
		chrBank[7] = chrBank[0] + 7;
		chr_bank_switch();
	}
}

/*-----------------------------------BANDAI------------------------------------*/

/*/////////////////////////////////*/
/*               FCG               */
/*/////////////////////////////////*/

/*/////////////////////////////////*/
/*             LZ93D50             */
/*/////////////////////////////////*/

/* requires EEPROM emulation and registers at 0x6000-0x7000:
 * http://wiki.nesdev.com/w/index.php/Bandai_FCG_board
 * http://forums.nesdev.com/viewtopic.php?f=3&t=9606&p=104643&hilit=I%C2%B2C#p104643
 */

/*----------------------------------Bit Corp.------------------------------------*/

static inline void mapper_bitcorp(uint_fast16_t, uint_fast8_t), reset_bitcorp();

void mapper_bitcorp (uint_fast16_t address, uint_fast8_t value)
{
	if ((address & 0x7fff) >= 0x7000)
	{
	prgBank[0] = ((value & 0x03) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();

	chrBank[0] = (((value >> 2) & 0x03) << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
	}
}

void reset_bitcorp()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = 4;
	prgBank[5] = 5;
	prgBank[6] = 6;
	prgBank[7] = 7;
	prg_bank_switch();
}

/*-----------------------------------CODEMASTERS------------------------------------*/

/*/////////////////////////////////*/
/*              BF9093			   */
/* 				BF9096             */
/* 				BF9097             */
/*/////////////////////////////////*/

/* TODO:
 * bf9096 - correct banking.... (quattro games)
 */

static inline void mapper_bf909x(uint_fast16_t, uint_fast8_t);
static uint_fast8_t bf909xOuter = 0;
void mapper_bf909x(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xe000)
	{
	case 0x8000:
		if (cart.mirroring == 5)
		{
			printf("%02x\n",value);
			nameSlot[0] = (value & 0x10) ? (ciram + 0x400) : ciram;
			nameSlot[1] = nameSlot[0];
			nameSlot[2] = nameSlot[0];
			nameSlot[3] = nameSlot[0];
		}
		if (!strcmp(cart.slot,"bf9096"))
		{
			printf("Outer bank: %02x\n",value&0x18);
			bf909xOuter = ((value & 0x18) << 1);
		}
		break;
	case 0xa000:
		if (!strcmp(cart.slot,"bf9096"))
		{
			printf("Outer bank: %02x\n",value&0x18);
			bf909xOuter = ((value & 0x18) << 1);
		}
		break;
	case 0xc000:
	case 0xe000:
		if (!strcmp(cart.slot,"bf9096"))
		{
			printf("Inner bank: %02x\n",value&0x03);
			prgBank[0] = ((value & 0x03) << 2) + bf909xOuter;
			prgBank[1] = prgBank[0] + 1;
			prgBank[2] = prgBank[0] + 2;
			prgBank[3] = prgBank[0] + 3;
		}
		else
		{
			prgBank[0] = ((value & 0x0f) << 2);
			prgBank[1] = prgBank[0] + 1;
			prgBank[2] = prgBank[0] + 2;
			prgBank[3] = prgBank[0] + 3;
		}
		prg_bank_switch();
		break;
	}
}

/*-----------------------------------COLOR DREAMS------------------------------------*/

/*/////////////////////////////////*/
/*              74x377             */
/*/////////////////////////////////*/

static inline void mapper_74x377(uint_fast16_t, uint_fast8_t), reset_74x377();

void mapper_74x377(uint_fast16_t address, uint_fast8_t value)
{
	prgBank[0] = ((value & 0x03) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	chrBank[0] = ((value >> 4) << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	prg_bank_switch();
	chr_bank_switch();
}

void reset_74x377()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = 4;
	prgBank[5] = 5;
	prgBank[6] = 6;
	prgBank[7] = 7;
	prg_bank_switch();
}

/*-----------------------------------IREM------------------------------------*/

/*/////////////////////////////////*/
/*               BNROM             */
/*/////////////////////////////////*/

static inline void mapper_bnrom(uint_fast16_t, uint_fast8_t);
void mapper_bnrom(uint_fast16_t address, uint_fast8_t value)
{
	prgBank[0] = ((value & 0x03) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();
}

/*/////////////////////////////////*/
/*               G-101             */
/*/////////////////////////////////*/

static inline void mapper_g101(uint_fast16_t, uint_fast8_t);
static uint_fast8_t g101Prg0, g101PrgMode;

void mapper_g101(uint_fast16_t address, uint_fast8_t value)
{
	if ((address & 0xf007) >= 0x8000 && (address & 0xf007) < 0x9000) {
		g101Prg0 = (value << 1);
		if (g101PrgMode) {
			prgBank[0] = cart.pSlots - 4;
			prgBank[1] = cart.pSlots - 3;
			prgBank[4] = g101Prg0;
			prgBank[5] = g101Prg0 + 1;
		} else {
			prgBank[0] = g101Prg0;
			prgBank[1] = g101Prg0 + 1;
			prgBank[4] = cart.pSlots - 4;
			prgBank[5] = cart.pSlots - 3;
		}
		prg_bank_switch();
	} else if ((address & 0xf007) >= 0x9000 && (address & 0xf007) < 0xa000) {
		g101PrgMode = (value & 0x02);
		if (!(cart.mirroring == 3))
		{
			cart.mirroring = (value & 0x01) ? 0 : 1;
			nametable_mirroring(cart.mirroring);
		}
		if (g101PrgMode) {
			prgBank[0] = cart.pSlots - 4;
			prgBank[1] = cart.pSlots - 3;
			prgBank[4] = g101Prg0;
			prgBank[5] = g101Prg0 + 1;
		} else {
			prgBank[0] = g101Prg0;
			prgBank[1] = g101Prg0 + 1;
			prgBank[4] = cart.pSlots - 4;
			prgBank[5] = cart.pSlots - 3;
		}
		prg_bank_switch();
	} else if ((address & 0xf007) >= 0xa000 && (address & 0xf007) < 0xb000) {
		prgBank[2] = (value << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
	} else if ((address & 0xf007) == 0xb000) {
		chrBank[0] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb001) {
		chrBank[1] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb002) {
		chrBank[2] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb003) {
		chrBank[3] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb004) {
		chrBank[4] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb005) {
		chrBank[5] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb006) {
		chrBank[6] = value;
		chr_bank_switch();
	} else if ((address & 0xf007) == 0xb007) {
		chrBank[7] = value;
		chr_bank_switch();
	}
}

/*/////////////////////////////////*/
/*              H3001              */
/*/////////////////////////////////*/

static inline void mapper_h3001(uint_fast16_t, uint_fast8_t), reset_h3001(), h3001_irq();
static uint_fast8_t h3001IrqEnable;
static uint_fast16_t h3001IrqReload, h3001IrqCounter;
void mapper_h3001(uint_fast16_t address, uint_fast8_t value)
{
 switch (address & 0xf007)
 {
 case 0x8000:
	 prgBank[0] = (value << 1);
	 prgBank[1] = prgBank[0] + 1;
	 prg_bank_switch();
	 break;
 case 0xa000:
	 prgBank[2] = (value << 1);
	 prgBank[3] = prgBank[2] + 1;
	 prg_bank_switch();
	 break;
 case 0xc000:
	 prgBank[4] = (value << 1);
	 prgBank[5] = prgBank[4] + 1;
	 prg_bank_switch();
	 break;
 case 0xb000:
	 chrBank[0] = value;
	 chr_bank_switch();
	 break;
 case 0xb001:
	 chrBank[1] = value;
	 chr_bank_switch();
	 break;
 case 0xb002:
	 chrBank[2] = value;
	 chr_bank_switch();
	 break;
 case 0xb003:
	 chrBank[3] = value;
	 chr_bank_switch();
	 break;
 case 0xb004:
	 chrBank[4] = value;
	 chr_bank_switch();
	 break;
 case 0xb005:
	 chrBank[5] = value;
	 chr_bank_switch();
	 break;
 case 0xb006:
	 chrBank[6] = value;
	 chr_bank_switch();
	 break;
 case 0xb007:
	 chrBank[7] = value;
	 chr_bank_switch();
	 break;
 case 0x9001:
	 cart.mirroring = 1 - (value >> 7);
	 nametable_mirroring(cart.mirroring);
	 break;
 case 0x9003:
	 h3001IrqEnable = (value & 0x80);
	 mapperInt = 0;
	 break;
 case 0x9004:
	 h3001IrqCounter = h3001IrqReload;
	 mapperInt = 0;
	 break;
 case 0x9005:
	 h3001IrqReload = ((h3001IrqReload & 0x00ff) | (value << 8));
	 break;
 case 0x9006:
	 h3001IrqReload = ((h3001IrqReload & 0xff00) | value);
	 break;
 }
}

void h3001_irq()
{
 if (h3001IrqEnable)
 {
	 if (!h3001IrqCounter)
		 mapperInt = 1;
	 else
		 h3001IrqCounter--;
 }
}

void reset_h3001()
{
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = 0x3c;
	prgBank[5] = 0x3d;
	prg_bank_switch();
}

/*/////////////////////////////////*/
/*           Holy Diver            */
/*/////////////////////////////////*/

static inline void mapper_holydivr(uint_fast16_t, uint_fast8_t);

void mapper_holydivr(uint_fast16_t address, uint_fast8_t value)
{
	prgBank[0] = ((value & 0x7) << 2);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prg_bank_switch();
	nametable_mirroring((value >> 3) & 1);
	chrBank[0] = ((value >> 4) << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
}

/*/////////////////////////////////*/
/*             lrog017             */
/*/////////////////////////////////*/

static inline void mapper_lrog017(uint_fast16_t, uint_fast8_t), reset_lrog017();

void mapper_lrog017(uint_fast16_t address, uint_fast8_t value)
{
	chrBank[0] = ((value >> 4) << 1);
	chrBank[1] = chrBank[0] + 1;
	chr_bank_switch();
	prgBank[0] = ((value & 0xf) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();
}

void reset_lrog017()
{
	chrSource[0] = CHR_ROM;
	chrSource[1] = CHR_ROM;
	chrSource[2] = CHR_RAM;
	chrSource[3] = CHR_RAM;
	chrSource[4] = CHR_RAM;
	chrSource[5] = CHR_RAM;
	chrSource[6] = CHR_RAM;
	chrSource[7] = CHR_RAM;
	chrBank[0] = 0;
	chrBank[1] = 1;
	chrBank[2] = 0;
	chrBank[3] = 1;
	chrBank[4] = 2;
	chrBank[5] = 3;
	chrBank[6] = 4;
	chrBank[7] = 5;
	chr_bank_switch();
	nameSlot[0] = chrRam + 0x1800;
	nameSlot[1] = chrRam + 0x1c00;
	nameSlot[2] = ciram;
	nameSlot[3] = ciram + 0x400;
}

/*-----------------------------------JALECO------------------------------------*/

/*/////////////////////////////////*/
/*               JF-16             */
/*/////////////////////////////////*/

static inline void mapper_jf16(uint_fast16_t, uint_fast8_t);

void mapper_jf16(uint_fast16_t address, uint_fast8_t value)
{
	prgBank[0] = ((value & 0x7) << 2);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prg_bank_switch();
	nametable_mirroring(((value >> 3) & 1) + 2);
	chrBank[0] = ((value >> 4) << 3);
	chrBank[1] = chrBank[0] + 1;
	chrBank[2] = chrBank[0] + 2;
	chrBank[3] = chrBank[0] + 3;
	chrBank[4] = chrBank[0] + 4;
	chrBank[5] = chrBank[0] + 5;
	chrBank[6] = chrBank[0] + 6;
	chrBank[7] = chrBank[0] + 7;
	chr_bank_switch();
}

/*/////////////////////////////////*/
/*               JF-17             */
/*/////////////////////////////////*/

/* TODO:
 * expansion sound for jf17pcm
 */

static inline void mapper_jf17(uint_fast16_t, uint_fast8_t);
uint_fast8_t jf17PrgSelect, jf17ChrSelect;
void mapper_jf17(uint_fast16_t address, uint_fast8_t value)
{
 if ((value & 0x80) && !jf17PrgSelect)
 {
	 prgBank[0] = ((value & 0x07) << 2);
	 prgBank[1] = prgBank[0] + 1;
	 prgBank[2] = prgBank[0] + 2;
	 prgBank[3] = prgBank[0] + 3;
	 prg_bank_switch();
 }
 if ((value & 0x40) && !jf17ChrSelect)
 {
	 chrBank[0] = ((value & 0x0f) << 3);
	 chrBank[1] = chrBank[0] + 1;
	 chrBank[2] = chrBank[0] + 2;
	 chrBank[3] = chrBank[0] + 3;
	 chrBank[4] = chrBank[0] + 4;
	 chrBank[5] = chrBank[0] + 5;
	 chrBank[6] = chrBank[0] + 6;
	 chrBank[7] = chrBank[0] + 7;
	 chr_bank_switch();
 }
 jf17PrgSelect = (value & 0x80);
 jf17ChrSelect = (value & 0x40);
}

/*/////////////////////////////////*/
/*             ss88006             */
/*/////////////////////////////////*/

static uint_fast8_t ss88006Prg0, ss88006Prg1, ss88006Prg2, ss88006IrqControl;
static uint_fast16_t ss88006IrqCounter, ss88006IrqReload;
static inline void mapper_ss88006(uint_fast16_t, uint_fast8_t);

void mapper_ss88006(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xf003)
	{
	case 0x8000:
		ss88006Prg0 = ((ss88006Prg0 & 0xf0) | (value & 0x0f));
		prgBank[0] = (ss88006Prg0 << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
		break;
	case 0x8001:
		ss88006Prg0 = ((ss88006Prg0 & 0x0f) | ((value & 0x0f) << 4));
		prgBank[0] = (ss88006Prg0 << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
		break;
	case 0x8002:
		ss88006Prg1 = ((ss88006Prg1 & 0xf0) | (value & 0x0f));
		prgBank[2] = (ss88006Prg1 << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
		break;
	case 0x8003:
		ss88006Prg1 = ((ss88006Prg1 & 0x0f) | ((value & 0x0f) << 4));
		prgBank[2] = (ss88006Prg1 << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
		break;
	case 0x9000:
		ss88006Prg2 = ((ss88006Prg2 & 0xf0) | (value & 0x0f));
		prgBank[4] = (ss88006Prg2 << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
		break;
	case 0x9001:
		ss88006Prg2 = ((ss88006Prg2 & 0x0f) | ((value & 0x0f) << 4));
		prgBank[4] = (ss88006Prg2 << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
		break;
	case 0x9002:
		/* TODO: differ between read and write enable */
		wramEnable = (value & 0x03);
		break;
	case 0xa000:
		chrBank[0] = ((chrBank[0] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xa001:
		chrBank[0] = ((chrBank[0] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xa002:
		chrBank[1] = ((chrBank[1] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xa003:
		chrBank[1] = ((chrBank[1] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xb000:
		chrBank[2] = ((chrBank[2] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xb001:
		chrBank[2] = ((chrBank[2] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xb002:
		chrBank[3] = ((chrBank[3] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xb003:
		chrBank[3] = ((chrBank[3] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xc000:
		chrBank[4] = ((chrBank[4] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xc001:
		chrBank[4] = ((chrBank[4] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xc002:
		chrBank[5] = ((chrBank[5] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xc003:
		chrBank[5] = ((chrBank[5] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xd000:
		chrBank[6] = ((chrBank[6] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xd001:
		chrBank[6] = ((chrBank[6] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xd002:
		chrBank[7] = ((chrBank[7] & 0xf0) | (value & 0x0f));
		chr_bank_switch();
		break;
	case 0xd003:
		chrBank[7] = ((chrBank[7] & 0x0f) | ((value & 0x0f) << 4));
		chr_bank_switch();
		break;
	case 0xe000:
		ss88006IrqReload = ((ss88006IrqReload & 0xfff0) | (value & 0x000f));
		break;
	case 0xe001:
		ss88006IrqReload = ((ss88006IrqReload & 0xff0f) | ((value & 0x000f) << 4));
		break;
	case 0xe002:
		ss88006IrqReload = ((ss88006IrqReload & 0xf0ff) | ((value & 0x000f) << 8));
		break;
	case 0xe003:
		ss88006IrqReload = ((ss88006IrqReload & 0x0fff) | ((value & 0x000f) << 12));
		break;
	case 0xf000: /* IRQ reset */
		ss88006IrqCounter = ss88006IrqReload;
		mapperInt = 0;
		break;
	case 0xf001: /* IRQ control */
		ss88006IrqControl = value;
		mapperInt = 0;
		break;
	case 0xf002:
		cart.mirroring = (value & 0x03);
		nametable_mirroring(cart.mirroring);
		break;
	case 0xf003: /* Sound control */
		/* TODO: ADPCM sound support:
		 * http://forums.nesdev.com/viewtopic.php?t=762
		 * http://forums.nesdev.com/viewtopic.php?p=32572#p32572
		 */
		printf("Writing %02x to unimplemented register 0xf003\n",value);
		break;
	}
}

void ss88006_irq()
{
	if (ss88006IrqControl & 0x01)
	{
	uint_fast16_t mask = (((ss88006IrqControl & 0x8) ? 0 : 0xf000) | ((ss88006IrqControl & 0x4) ? 0 : 0x0f00) | ((ss88006IrqControl & 0x2) ? 0 : 0x00f0) | 0xf);
	int tmpCounter = (ss88006IrqCounter & mask);

	tmpCounter--;
	if (tmpCounter < 0)
	{
		ss88006IrqCounter = ss88006IrqReload;
		mapperInt = 1;
	}
	else
		ss88006IrqCounter = ((ss88006IrqCounter & ~mask) | tmpCounter);
	}
}

/*-----------------------------------KONAMI------------------------------------*/

static inline void vrc_clock_irq();
static uint_fast8_t vrcIrqControl = 0, vrcIrqLatch, vrcIrqCounter, vrcIrqCycles[3] = { 114, 114, 113 }, vrcIrqCc = 0;
static int16_t vrcIrqPrescale;

/*/////////////////////////////////*/
/*           Konami VRC 1          */
/*/////////////////////////////////*/

uint_fast8_t vrc1Chr0, vrc1Chr1;
static inline void mapper_vrc1(uint_fast16_t, uint_fast8_t);

void mapper_vrc1(uint_fast16_t address, uint_fast8_t value)
{
	if (address >= 0x8000 && address <= 0x8fff) /* PRG select 0 */
	{
		prgBank[0] = ((value & 0x0f) << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
	}
	if (address >= 0xa000 && address <= 0xafff) /* PRG select 1 */
	{
		prgBank[2] = ((value & 0x0f) << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
	}
	if (address >= 0xc000 && address <= 0xcfff) /* PRG select 2 */
	{
		prgBank[4] = ((value & 0x0f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
	}
	if (address >= 0x9000 && address <= 0x9fff) /* Mirroring + CHR */
	{
		cart.mirroring = (1 - (value & 0x01));
		nametable_mirroring(cart.mirroring);
		vrc1Chr0 = ((vrc1Chr0 & 0x0f) | ((value & 0x02) << 3));
		vrc1Chr1 = ((vrc1Chr1 & 0x0f) | ((value & 0x04) << 2));
		chrBank[0] = (vrc1Chr0 << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chrBank[4] = (vrc1Chr1 << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
		chr_bank_switch();
	}
	if (address >= 0xe000 && address <= 0xefff) /* CHR select 0 */
	{
		vrc1Chr0 = ((vrc1Chr0 & 0x10) | (value & 0x0f));
		chrBank[0] = (vrc1Chr0 << 2);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = chrBank[0] + 2;
		chrBank[3] = chrBank[0] + 3;
		chr_bank_switch();
	}
	if (address >= 0xf000 && address <= 0xffff) /* CHR select 1 */
	{
		vrc1Chr1 = ((vrc1Chr1 & 0x10) | (value & 0x0f));
		chrBank[4] = (vrc1Chr1 << 2);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = chrBank[4] + 2;
		chrBank[7] = chrBank[4] + 3;
		chr_bank_switch();
	}
}

/*/////////////////////////////////*/
/*          Konami VRC 2           */
/* 		    Konami VRC 4           */
/*/////////////////////////////////*/

static uint_fast8_t vrc24SwapMode = 0;
uint_fast8_t wramBit = 0, wramBitVal;

static inline void mapper_vrc24(uint_fast16_t, uint_fast8_t);

void mapper_vrc24(uint_fast16_t address, uint_fast8_t value) {
	/* reroute addressing */
	if (cart.vrc24Prg1 > 1)
		address = (address & 0xff00) | ((address>>(cart.vrc24Prg1-1)) & 0x02) | ((address>>cart.vrc24Prg0) & 0x01);
	else
		address = (address & 0xff00) | ((address<<(1-cart.vrc24Prg1)) & 0x02) | ((address>>cart.vrc24Prg0) & 0x01);

	/* handle register writes */
	if ((address&0xf003) >= 0x8000 && (address&0xf003) <= 0x8003) { /* PRG select 0 */
		if (vrc24SwapMode)
		{
			prgBank[0] = cart.pSlots - 4;
			prgBank[1] = prgBank[0] + 1;
			prgBank[4] = (value << 1);
			prgBank[5] = prgBank[4] + 1;
		}
		else
		{
			prgBank[0] = (value << 1);
			prgBank[1] = prgBank[0] + 1;
			prgBank[4] = cart.pSlots - 4;
			prgBank[5] = prgBank[4] + 1;
		}
		prg_bank_switch();
	} else if ((address&0xf003) >= 0xa000  && (address&0xf003) <= 0xa003) { /* PRG select 1 */
		prgBank[2] = (value << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
	} else if ((address&0xf003) >= 0x9000  && (address&0xf003) <= 0x9003) { /* mirroring control */
		if (!strcmp(cart.slot,"vrc4") && (address&0xf003) >= 0x9002) {
			vrc24SwapMode = ((value >> 1) & 0x01);
			if (vrc24SwapMode)
			{
				prgBank[0] = cart.pSlots - 4;
				prgBank[1] = prgBank[0] + 1;
				prgBank[4] = (value << 1);
				prgBank[5] = prgBank[4] + 1;
			}
			else
			{
				prgBank[0] = (value << 1);
				prgBank[1] = prgBank[0] + 1;
				prgBank[4] = cart.pSlots - 4;
				prgBank[5] = prgBank[4] + 1;
			}
			prg_bank_switch();
		} else if (!strcmp(cart.slot,"vrc4") && (address&0xf003) < 0x9002) {
			switch (value & 0x03) {
			case 0:
				cart.mirroring = 1;
				break;
			case 1:
				cart.mirroring = 0;
				break;
			case 2:
				cart.mirroring = 2;
				break;
			case 3:
				cart.mirroring = 3;
				break;
			}
			nametable_mirroring(cart.mirroring);
		} else if (!strcmp(cart.slot,"vrc2")) {
			cart.mirroring = (value&1) ? 0 : 1;
			nametable_mirroring(cart.mirroring);
		}
	} else if ((address&0xf003) == 0xb000) { /* CHR select 0 low */
		chrBank[0] = (chrBank[0] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xb001) { /* CHR select 0 high */
		chrBank[0] = (chrBank[0] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xb002) { /* CHR select 1 low */
		chrBank[1] = (chrBank[1] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xb003) { /* CHR select 1 high */
		chrBank[1] = (chrBank[1] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xc000) { /* CHR select 2 low */
		chrBank[2] = (chrBank[2] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xc001) { /* CHR select 2 high */
		chrBank[2] = (chrBank[2] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xc002) { /* CHR select 3 low */
		chrBank[3] = (chrBank[3] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xc003) { /* CHR select 3 high */
		chrBank[3] = (chrBank[3] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xd000) { /* CHR select 4 low */
		chrBank[4] = (chrBank[4] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xd001) { /* CHR select 4 high */
		chrBank[4] = (chrBank[4] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xd002) { /* CHR select 5 low */
		chrBank[5] = (chrBank[5] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xd003) { /* CHR select 5 high */
		chrBank[5] = (chrBank[5] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xe000) { /* CHR select 6 low */
		chrBank[6] = (chrBank[6] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xe001) { /* CHR select 6 high */
		chrBank[6] = (chrBank[6] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xe002) { /* CHR select 7 low */
		chrBank[7] = (chrBank[7] & 0x1f0) | (value & 0xf);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xe003) { /* CHR select 7 high */
		chrBank[7] = (chrBank[7] & 0xf) | ((value & 0x1f) << 4);
		chr_bank_switch();
	} else if ((address&0xf003) == 0xf000) { /* IRQ Latch low */
		vrcIrqLatch = (vrcIrqLatch & 0xf0) | (value & 0x0f);
	} else if ((address&0xf003) == 0xf001) { /* IRQ Latch high */
		vrcIrqLatch = (vrcIrqLatch & 0x0f) | ((value & 0x0f) << 4);
	} else if ((address&0xf003) == 0xf002) { /* IRQ Control */
		vrcIrqControl = (value & 0x07);
		if (vrcIrqControl & 0x02) {
			vrcIrqCounter = vrcIrqLatch;
			vrcIrqPrescale = vrcIrqCycles[0];
			vrcIrqCc = 0;
		}
		mapperInt = 0;
	} else if ((address&0xf003) == 0xf003) { /* IRQ Acknowledge */
		mapperInt = 0;
		vrcIrqControl = ((vrcIrqControl & 0x04) | ((vrcIrqControl & 0x01) << 1) | (vrcIrqControl & 0x01));
	}
}

/*/////////////////////////////////*/
/*              VRC 3              */
/*/////////////////////////////////*/

static inline void mapper_vrc3(uint_fast16_t, uint_fast8_t), vrc3_irq(void);
static uint_fast8_t vrc3IrqControl;
static uint_fast16_t vrc3IrqCounter, vrc3IrqLatch;
void mapper_vrc3(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xf000)
	{
	case 0xf000: /* PRG Select */
		prgBank[0] = ((value & 0x07) << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prg_bank_switch();
		break;
	case 0x8000: /* IRQ Latch 0 */
		vrc3IrqLatch = ((vrc3IrqLatch & 0xfff0) | (value & 0x0f));
		break;
	case 0x9000: /* IRQ Latch 1 */
		vrc3IrqLatch = ((vrc3IrqLatch & 0xff0f) | ((value & 0x0f) << 4));
		break;
	case 0xa000: /* IRQ Latch 2 */
		vrc3IrqLatch = ((vrc3IrqLatch & 0xf0ff) | ((value & 0x0f) << 8));
		break;
	case 0xb000: /* IRQ Latch 3 */
		vrc3IrqLatch = ((vrc3IrqLatch & 0x0fff) | ((value & 0x0f) << 12));
		break;
	case 0xc000: /* IRQ Control */
		vrc3IrqControl = (value & 0x07);
		if (vrc3IrqControl & 0x02) {
			vrc3IrqCounter = vrc3IrqLatch;
		}
		mapperInt = 0;
		break;
	case 0xd000: /* IRQ Acknowledge */
		mapperInt = 0;
		vrc3IrqControl = ((vrc3IrqControl & 0x04) | ((vrc3IrqControl & 0x01) << 1) | (vrc3IrqControl & 0x01));
		break;
	default:
		printf("Unhandled write of %02x to %04x\n",value,address);
		break;
	}
}

void vrc3_irq() {
	if ((vrc3IrqControl & 0x02))
	{
		if (!(vrc3IrqControl & 0x04)) /* 16-bit mode */
		{
			if (!(vrc3IrqCounter & 0xffff))
			{
				mapperInt = 1;
				vrc3IrqCounter = vrc3IrqLatch;
			}
			else
				vrc3IrqCounter++;
		}
		else if (vrc3IrqControl & 0x04) /* 8-bit mode */
		{
			if (!(vrc3IrqCounter & 0xff))
			{
				mapperInt = 1;
				vrc3IrqCounter = ((vrc3IrqCounter & 0xff00) | (vrc3IrqLatch & 0x00ff));
			}
			else
				vrc3IrqCounter = ((vrc3IrqCounter & 0xff00) | ((vrc3IrqCounter + 1) & 0x00ff));
		}
	}
}

/*/////////////////////////////////*/
/*              VRC 6              */
/*/////////////////////////////////*/

static uint_fast8_t vrc6Pulse1Mode, vrc6Pulse1Duty, vrc6Pulse1Volume, vrc6Pulse2Mode, vrc6Pulse2Duty, vrc6Pulse2Volume,
					vrc6SawAccumulator,	vrc6Pulse1Enable, vrc6Pulse2Enable, vrc6SawEnable, vrc6Pulse1DutyCounter, vrc6Pulse2DutyCounter,
					vrc6SawAccCounter = 0, vrc6SawAcc = 0;
static uint_fast16_t vrc6Pulse1Period, vrc6Pulse2Period, vrc6SawPeriod, vrc6Pulse1Counter = 0, vrc6Pulse2Counter = 0, vrc6SawCounter = 0;
static inline void mapper_vrc6(uint_fast16_t, uint_fast8_t);

void mapper_vrc6(uint_fast16_t address, uint_fast8_t value)
{
	address = (address & 0xff00) | ((address<<(1-cart.vrc6Prg1)) & 0x02) | ((address>>cart.vrc6Prg0) & 0x01);
	if ((address&0xf003) >= 0x8000 && (address&0xf003) <= 0x8003) /* 16k PRG select */
	{
		prgBank[0] = ((value & 0x0f) << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prg_bank_switch();
	}
	else if ((address&0xf003) >= 0xc000 && (address&0xf003) <= 0xc003) /* 8k PRG select */
	{
		prgBank[4] = ((value & 0x1f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
	}
	else if ((address&0xf003) == 0xb003) /* PPU banking */
	{
	/*	printf("Mirroring: %02x, %i,%i\n",value,scanline,ppudot); */
		switch (value & 0x0c)
		{
		case 0x00:
			cart.mirroring = 1;
			break;
		case 0x04:
			cart.mirroring = 0;
			break;
		case 0x08:
			cart.mirroring = 2;
			break;
		case 0x0c:
			cart.mirroring = 3;
			break;
		}
		nametable_mirroring(cart.mirroring);
		wramEnable = (value >> 7);
	}
	else if ((address&0xf003) == 0xd000) /* CHR select */
	{
		chrBank[0] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd001) /* CHR select */
	{
		chrBank[1] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd002) /* CHR select */
	{
		chrBank[2] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xd003) /* CHR select */
	{
		chrBank[3] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe000) /* CHR select */
	{
		chrBank[4] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe001) /* CHR select */
	{
		chrBank[5] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe002) /* CHR select */
	{
		chrBank[6] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xe003) /* CHR select */
	{
		chrBank[7] = value;
		chr_bank_switch();
	}
	else if ((address&0xf003) == 0xf000) /* IRQ Latch */
	{
		vrcIrqLatch = value;
	}
	else if ((address&0xf003) == 0xf001) /* IRQ Control */
	{
		vrcIrqControl = (value & 0x07);
		if (vrcIrqControl & 0x02)
		{
			vrcIrqCounter = vrcIrqLatch;
			vrcIrqPrescale = vrcIrqCycles[0];
			vrcIrqCc = 0;
		}
		mapperInt = 0;
	}
	else if ((address&0xf003) == 0xf002) /* IRQ Acknowledge */
	{
		mapperInt = 0;
		vrcIrqControl = ((vrcIrqControl & 0x04) | ((vrcIrqControl & 0x01) << 1) | (vrcIrqControl & 0x01));
	}
	else if ((address&0xf003) == 0x9000) /* Pulse 1 control */
	{
		vrc6Pulse1Mode = (value & 0x80);
		vrc6Pulse1Duty = ((value >> 4) & 7);
		vrc6Pulse1Volume = (value & 0xf);
	}
	else if ((address&0xf003) == 0x9001) /* Pulse 1 period low */
	{
		vrc6Pulse1Period = ((vrc6Pulse1Period & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0x9002) /* Pulse 1 period high */
	{
		vrc6Pulse1Period = ((vrc6Pulse1Period & 0x00ff) | ((value & 0xf) << 8));
		vrc6Pulse1Enable = (value & 0x80);
		if (!vrc6Pulse1Enable)
			vrc6Pulse1DutyCounter = 15;
	}
	else if ((address&0xf003) == 0x9003) /* Pulse 1 frequency */
	{
		/* unused by commerical games */
	}
	else if ((address&0xf003) == 0xa000) /* Pulse 2 control */
	{
		vrc6Pulse2Mode = (value & 0x80);
		vrc6Pulse2Duty = ((value >> 4) & 7);
		vrc6Pulse2Volume = (value & 0xf);
	}
	else if ((address&0xf003) == 0xa001) /* Pulse 2 period low */
	{
		vrc6Pulse2Period = ((vrc6Pulse2Period & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0xa002) /* Pulse 2 period high */
	{
		vrc6Pulse2Period = ((vrc6Pulse2Period & 0x00ff) | ((value & 0xf) << 8));
		vrc6Pulse2Enable = (value & 0x80);
		if (!vrc6Pulse2Enable)
			vrc6Pulse2DutyCounter = 15;
	}
	else if ((address&0xf003) == 0xa003) /* Pulse 2 frequency */
	{
		/* unused by commerical games */
	}
	else if ((address&0xf003) == 0xb000) /* Saw accumulator */
	{
		vrc6SawAccumulator = (value & 0x3f);
	}
	else if ((address&0xf003) == 0xb001) /* Saw period low */
	{
		vrc6SawPeriod = ((vrc6SawPeriod & 0x0f00) | value);
	}
	else if ((address&0xf003) == 0xb002) /* Saw period high */
	{
		vrc6SawPeriod = ((vrc6SawPeriod & 0x00ff) | ((value & 0xf) << 8));
		vrc6SawEnable = (value & 0x80);
		if (!vrc6SawEnable)
		{
			vrc6SawAccCounter = 0;
			vrc6SawAcc = 0;
		}
	}
}

float vrc6_sound()
{
	uint_fast8_t sample1 = 0;
	uint_fast8_t sample2 = 0;
	uint_fast8_t sample3 = (vrc6SawAcc >> 3);

	if (((vrc6Pulse1DutyCounter <= vrc6Pulse1Duty) || vrc6Pulse1Mode) && vrc6Pulse1Enable)
		sample1 = vrc6Pulse1Volume;
	if (!vrc6Pulse1Counter)
	{
		vrc6Pulse1Counter = vrc6Pulse1Period;
		if (!vrc6Pulse1DutyCounter)
			vrc6Pulse1DutyCounter = 15;
		else if (vrc6Pulse1Enable)
		{
			vrc6Pulse1DutyCounter--;
		}
	}
	else
		vrc6Pulse1Counter--;

	if (((vrc6Pulse2DutyCounter <= vrc6Pulse2Duty) || vrc6Pulse2Mode) && vrc6Pulse2Enable)
			sample2 = vrc6Pulse2Volume;
	if (!vrc6Pulse2Counter)
	{
		vrc6Pulse2Counter = vrc6Pulse2Period;
		if (!vrc6Pulse2DutyCounter)
			vrc6Pulse2DutyCounter = 15;
		else if (vrc6Pulse2Enable)
		{
			vrc6Pulse2DutyCounter--;
		}
	}
	else
		vrc6Pulse2Counter--;

	if (!vrc6SawCounter)
	{
		vrc6SawCounter = vrc6SawPeriod;
		if (vrc6SawAccCounter == 13)
		{
			vrc6SawAccCounter = 0;
			vrc6SawAcc = 0;
		}
		else if (vrc6SawEnable)
		{
			vrc6SawAccCounter++;
			if (!(vrc6SawAccCounter%2))
			{
				vrc6SawAcc += vrc6SawAccumulator;
			}
		}
	}
	else
		vrc6SawCounter--;
	return ((sample1 + sample2 + sample3));
}

void vrc_clock_irq()
{
	if (vrcIrqCounter == 0xff) {
		mapperInt = 1;
		irqPulled = 1; /* otherwise gets read too late */
		vrcIrqCounter = vrcIrqLatch;
	}
	else
	{
		vrcIrqCounter++;
	}
}

void vrc_irq() {
	if ((vrcIrqControl & 0x02)) {
		if (!(vrcIrqControl & 0x04)) {
			vrcIrqPrescale--;
			if (vrcIrqPrescale == 0)
				{
				vrcIrqPrescale = vrcIrqCycles[vrcIrqCc++];
				if (vrcIrqCc == 3)
					vrcIrqCc = 0;
				vrc_clock_irq();
				}
		} else if (vrcIrqControl & 0x04) {
			vrc_clock_irq();
		}
	}
}

/*-----------------------------------NAMCO------------------------------------*/

/*/////////////////////////////////*/
/*            NAMCO 163            */
/*/////////////////////////////////*/

static inline void mapper_namco163(uint_fast16_t, uint_fast8_t),namco163_irq(), namco163_chr_bank_switch();

static uint_fast8_t namco163CramEnable0 = 0, namco163CramEnable1 = 0, namco163Chr0, namco163Chr1,
namco163Chr2, namco163Chr3, namco163Chr4, namco163Chr5, namco163Chr6, namco163Chr7, namco163IrqEnable;
static uint_fast16_t namco163IrqCounter = 0;

uint_fast8_t namco163_read (uint_fast16_t address)
{
	uint_fast8_t value = 0;
	switch (address & 0xf800)
	{
		case 0x5000:
			value = (namco163IrqCounter & 0xff);
			break;
		case 0x5800:
			value = ((namco163IrqCounter & 0x7f00) >> 8);
			break;
	}
	return value;
}
void mapper_namco163(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xf800)
	{
	case 0x4800: /* Chip RAM Data Port */
		printf("Writing %02x to unimplemented register 0x4800\n",value);
		break;
	case 0x5000: /* IRQ Counter (low) */
		namco163IrqCounter = ((namco163IrqCounter & 0xff00) | value);
		mapperInt = 0;
		break;
	case 0x5800: /* IRQ Counter (high + enable) */
		namco163IrqEnable = (value & 0x80);
		namco163IrqCounter = ((namco163IrqCounter & 0x00ff) | ((value & 0x7f) << 8));
		mapperInt = 0;
		break;
	case 0x8000: /* CHR 0 select */
		namco163Chr0 = value;
		namco163_chr_bank_switch();
		break;
	case 0x8800: /* CHR 1 select */
		namco163Chr1 = value;
		namco163_chr_bank_switch();
		break;
	case 0x9000: /* CHR 2 select */
		namco163Chr2 = value;
		namco163_chr_bank_switch();
		break;
	case 0x9800: /* CHR 3 select */
		namco163Chr3 = value;
		namco163_chr_bank_switch();
		break;
	case 0xa000: /* CHR 4 select */
		namco163Chr4 = value;
		namco163_chr_bank_switch();
		break;
	case 0xa800: /* CHR 5 select */
		namco163Chr5 = value;
		namco163_chr_bank_switch();
		break;
	case 0xb000: /* CHR 6 select */
		namco163Chr6 = value;
		namco163_chr_bank_switch();
		break;
	case 0xb800: /* CHR 7 select */
		namco163Chr7 = value;
		namco163_chr_bank_switch();
		break;
	case 0xc000: /* NT 0 select */
		if (value >= 0xe0)
			nameSlot[0] = ((value%2) ? ciram + 0x400 : ciram);
		else
			nameSlot[0] = &chrRom[((value & ((cart.chrSize >> 10) - 1)) << 10)];
		break;
	case 0xc800: /* NT 1 select */
		if (value >= 0xe0)
			nameSlot[1] = ((value%2) ? ciram + 0x400 : ciram);
		else
			nameSlot[1] = &chrRom[((value & ((cart.chrSize >> 10) - 1)) << 10)];
		break;
	case 0xd000: /* NT 2 select */
		if (value >= 0xe0)
			nameSlot[2] = ((value%2) ? ciram + 0x400 : ciram);
		else
			nameSlot[2] = &chrRom[((value & ((cart.chrSize >> 10) - 1)) << 10)];
		break;
	case 0xd800: /* NT 3 select */
		if (value >= 0xe0)
			nameSlot[3] = ((value%2) ? ciram + 0x400 : ciram);
		else
			nameSlot[3] = &chrRom[((value & ((cart.chrSize >> 10) - 1)) << 10)];
		break;
	case 0xe000: /* PRG 0 select  + sound enable */
		prgBank[0] = ((value & 0x3f) << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
		break;
	case 0xe800: /* PRG 1 select + CHR-RAM enable */
		namco163CramEnable0 = (value & 0x40);
		namco163CramEnable1 = (value & 0x80);
		prgBank[2] = ((value & 0x3f) << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
		break;
	case 0xf000: /* PRG 2 select */
		prgBank[4] = ((value & 0x3f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
		break;
	case 0xf800: /* RAM write protect + address port */
		printf("Writing %02x to unimplemented register 0xf800\n",value);
		break;
	}
}

void namco163_irq()
{
	if (namco163IrqEnable)
	{
		if (namco163IrqCounter == 0x7fff)
			mapperInt = 1;
		else
			namco163IrqCounter++;
	}
}

void namco163_chr_bank_switch()
{
	chrSlot[0] = (namco163CramEnable0 || (namco163Chr0 < 0xe0)) ? &chrRom[((namco163Chr0 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr0%2) ? ciram + 0x400 : ciram);
	chrSlot[1] = (namco163CramEnable0 || (namco163Chr1 < 0xe0)) ? &chrRom[((namco163Chr1 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr1%2) ? ciram + 0x400 : ciram);
	chrSlot[2] = (namco163CramEnable0 || (namco163Chr2 < 0xe0)) ? &chrRom[((namco163Chr2 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr2%2) ? ciram + 0x400 : ciram);
	chrSlot[3] = (namco163CramEnable0 || (namco163Chr3 < 0xe0)) ? &chrRom[((namco163Chr3 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr3%2) ? ciram + 0x400 : ciram);
	chrSlot[4] = (namco163CramEnable1 || (namco163Chr4 < 0xe0)) ? &chrRom[((namco163Chr4 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr4%2) ? ciram + 0x400 : ciram);
	chrSlot[5] = (namco163CramEnable1 || (namco163Chr5 < 0xe0)) ? &chrRom[((namco163Chr5 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr5%2) ? ciram + 0x400 : ciram);
	chrSlot[6] = (namco163CramEnable1 || (namco163Chr6 < 0xe0)) ? &chrRom[((namco163Chr6 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr6%2) ? ciram + 0x400 : ciram);
	chrSlot[7] = (namco163CramEnable1 || (namco163Chr7 < 0xe0)) ? &chrRom[((namco163Chr7 & ((cart.chrSize >> 10) - 1)) << 10)] : ((namco163Chr7%2) ? ciram + 0x400 : ciram);
}

/*/////////////////////////////////*/
/*           NAMCOT 34xx           */
/*/////////////////////////////////*/

static inline void mapper_namcot34xx(uint_fast16_t, uint_fast8_t), namcot34xx_bank_switch(void);
static uint_fast8_t namcot34xxSelect, namcot34xxReg[0x8];
void mapper_namcot34xx(uint_fast16_t address, uint_fast8_t value)
{
	if (cart.mirroring == 5)
	{
		int mirrorbit = (value & 0x40);
		nameSlot[0] = (ciram + (mirrorbit ? 0x400 : 0));
		nameSlot[1] = (ciram + (mirrorbit ? 0x400 : 0));
		nameSlot[2] = (ciram + (mirrorbit ? 0x400 : 0));
		nameSlot[3] = (ciram + (mirrorbit ? 0x400 : 0));
	}
	if ((address & 0xe001) < 0xa000)
	{
		if (!(address%2))  /* Bank select (0x8000) */
		{
			namcot34xxSelect = value;
		}
		else if (address%2) /* Bank data (0x8001) */
		{
			int bank = (namcot34xxSelect & 0x7);
			namcot34xxReg[bank] = value;
			namcot34xx_bank_switch();
		}
	}
}

void namcot34xx_bank_switch()
{
	if (!strcmp(cart.slot,"namcot_3425"))
	{
		nameSlot[0] = (namcot34xxReg[0] & 0x20) ? ciram : (ciram + 0x400);
		nameSlot[1] = (namcot34xxReg[0] & 0x20) ? ciram : (ciram + 0x400);
		nameSlot[2] = (namcot34xxReg[1] & 0x20) ? ciram : (ciram + 0x400);
		nameSlot[3] = (namcot34xxReg[1] & 0x20) ? ciram : (ciram + 0x400);
	}
	if (!strcmp(cart.slot,"namcot_3446"))
	{
		chrBank[0] = (namcot34xxReg[2] << 1);
		chrBank[1] = chrBank[0] + 1;
		chrBank[2] = (namcot34xxReg[3] << 1);
		chrBank[3] = chrBank[2] + 1;
		chrBank[4] = (namcot34xxReg[4] << 1);
		chrBank[5] = chrBank[4] + 1;
		chrBank[6] = (namcot34xxReg[5] << 1);
		chrBank[7] = chrBank[6] + 1;
	}
	else
	{
		chrBank[0] = (namcot34xxReg[0] & 0xfe);
		chrBank[1] = (namcot34xxReg[0] | 0x01);
		chrBank[2] = (namcot34xxReg[1] & 0xfe);
		chrBank[3] = (namcot34xxReg[1] | 0x01);
		chrBank[4] = (namcot34xxReg[2] | 0x40);
		chrBank[5] = (namcot34xxReg[3] | 0x40);
		chrBank[6] = (namcot34xxReg[4] | 0x40);
		chrBank[7] = (namcot34xxReg[5] | 0x40);
	}
	prgBank[0] = (namcot34xxReg[6] << 1);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = (namcot34xxReg[7] << 1);
	prgBank[3] = prgBank[2] + 1;
	chr_bank_switch();
	prg_bank_switch();
}

/*-----------------------------------RARE------------------------------------*/

/*/////////////////////////////////*/
/*               AxROM             */
/*/////////////////////////////////*/

/* TODO:
 *
 * -bus conflicts?
 *
 * Game specific:
 * -Battletoads: crashes at level 2 - timing issue
 * */
static inline void mapper_axrom(uint_fast16_t, uint_fast8_t);

void mapper_axrom(uint_fast16_t address, uint_fast8_t value) {
	prgBank[0] = ((value & 0x07) << 3);
	prgBank[1] = prgBank[0] + 1;
	prgBank[2] = prgBank[0] + 2;
	prgBank[3] = prgBank[0] + 3;
	prgBank[4] = prgBank[0] + 4;
	prgBank[5] = prgBank[0] + 5;
	prgBank[6] = prgBank[0] + 6;
	prgBank[7] = prgBank[0] + 7;
	prg_bank_switch();
	(value & 0x10) ? (cart.mirroring = 3) : (cart.mirroring = 2);
	nametable_mirroring(cart.mirroring);
}

/*---------------------------------SUNSOFT-----------------------------------*/

/*/////////////////////////////////*/
/*            Sunsoft 3            */
/*/////////////////////////////////*/

static inline void mapper_sun3(uint_fast16_t, uint_fast8_t), sun3_irq();
static uint_fast8_t sun3w, sun3IrqEnable;
static uint_fast16_t sun3IrqCounter;
void mapper_sun3(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xf800)
	{
	case 0x8800: /* CHR Bank 0 */
		chrBank[0] = (value << 1);
		chrBank[1] = chrBank[0] + 1;
		chr_bank_switch();
		break;
	case 0x9800: /* CHR Bank 1 */
		chrBank[2] = (value << 1);
		chrBank[3] = chrBank[2] + 1;
		chr_bank_switch();
		break;
	case 0xa800: /* CHR Bank 2 */
		chrBank[4] = (value << 1);
		chrBank[5] = chrBank[4] + 1;
		chr_bank_switch();
		break;
	case 0xb800: /* CHR Bank 3 */
		chrBank[6] = (value << 1);
		chrBank[7] = chrBank[6] + 1;
		chr_bank_switch();
		break;
	case 0xc800: /* IRQ Load   */
		if (sun3w)
			sun3IrqCounter = ((sun3IrqCounter & 0xff00) | value);
		else
			sun3IrqCounter = ((sun3IrqCounter & 0x00ff) | (value << 8));
		sun3w ^= 1;
		break;
	case 0xd800: /* IRQ Enable */
		sun3IrqEnable = (value & 0x10);
		sun3w = 0;
		mapperInt = 0;
		break;
	case 0xe800: /* Mirroring  */
		switch (value & 0x03)
		{
		case 0:
			cart.mirroring = 1;
			break;
		case 1:
			cart.mirroring = 0;
			break;
		case 2:
			cart.mirroring = 2;
			break;
		case 3:
			cart.mirroring = 3;
			break;
		}
		nametable_mirroring(cart.mirroring);
		break;
	case 0xf800: /* PRG Bank   */
		prgBank[0] = (value << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prg_bank_switch();
		break;
	}
}

void sun3_irq()
{
	if (sun3IrqEnable)
	{
	if ((sun3IrqCounter & 0xffff) == 0xffff)
		mapperInt = 1;
	else
		sun3IrqCounter--;
	}
}

/*/////////////////////////////////*/
/*            Sunsoft 4            */
/*/////////////////////////////////*/

static inline void mapper_sun4(uint_fast16_t, uint_fast8_t), sun4_nametable_mirroring(uint_fast8_t);
static uint_fast8_t sun4Name0, sun4Name1, sun4NameSelect;
void mapper_sun4(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xf000)
	{
	case 0x6000:
	case 0x7000: /* Licensing IC */
		/* TODO: The baseball game checks this (protection?)
		 * should be readable as well
		 */
		break;
	case 0x8000: /* CHR Bank 0 */
		chrBank[0] = (value << 1);
		chrBank[1] = chrBank[0] + 1;
		chr_bank_switch();
		break;
	case 0x9000: /* CHR Bank 1 */
		chrBank[2] = (value << 1);
		chrBank[3] = chrBank[2] + 1;
		chr_bank_switch();
		break;
	case 0xa000: /* CHR Bank 2 */
		chrBank[4] = (value << 1);
		chrBank[5] = chrBank[4] + 1;
		chr_bank_switch();
		break;
	case 0xb000: /* CHR Bank 3 */
		chrBank[6] = (value << 1);
		chrBank[7] = chrBank[6] + 1;
		chr_bank_switch();
		break;
	case 0xc000: /* Nametable 0 */
		sun4Name0 = (value | 0x80);
		if (sun4NameSelect)
			sun4_nametable_mirroring(cart.mirroring);
		break;
	case 0xd000: /* Nametable 1 */
		sun4Name1 = (value | 0x80);
		if (sun4NameSelect)
			sun4_nametable_mirroring(cart.mirroring);
		break;
	case 0xe000: /* Nametable Control */
		switch (value & 0x03)
		{
		case 0:
			cart.mirroring = 1;
			break;
		case 1:
			cart.mirroring = 0;
			break;
		case 2:
			cart.mirroring = 2;
			break;
		case 3:
			cart.mirroring = 3;
			break;
		}
		sun4NameSelect = (value & 0x10);
		if (!sun4NameSelect)
			nametable_mirroring(cart.mirroring);
		else
			sun4_nametable_mirroring(cart.mirroring);
		break;
	case 0xf000: /* PRG Bank 0 */
		wramEnable = (value & 0x10);
		prgBank[0] = ((value & 0x0f) << 2);
		prgBank[1] = prgBank[0] + 1;
		prgBank[2] = prgBank[0] + 2;
		prgBank[3] = prgBank[0] + 3;
		prg_bank_switch();
		break;
	}
}

void sun4_nametable_mirroring(uint_fast8_t mode)
{
	uint_fast8_t *low, *high;
	low = &chrRom[((sun4Name0 & ((cart.chrSize >> 10) - 1)) << 10)];
	high = &chrRom[((sun4Name1 & ((cart.chrSize >> 10) - 1)) << 10)];
	switch (mode)
	{
	case 0: /* horizontal */
		nameSlot[0] = low;
		nameSlot[1] = low;
		nameSlot[2] = high;
		nameSlot[3] = high;
		break;
	case 1: /* vertical */
		nameSlot[0] = low;
		nameSlot[1] = high;
		nameSlot[2] = low;
		nameSlot[3] = high;
		break;
	case 2: /* one-page low */
		nameSlot[0] = low;
		nameSlot[1] = low;
		nameSlot[2] = low;
		nameSlot[3] = low;
		break;
	case 3: /* one-page high */
		nameSlot[0] = high;
		nameSlot[1] = high;
		nameSlot[2] = high;
		nameSlot[3] = high;
		break;
	}
}

/*/////////////////////////////////*/
/*           Sunsoft 5a            */
/* 			 Sunsoft 5b			   */
/* 			 Sunsoft FME-7         */
/*/////////////////////////////////*/

/* TODO:
 *
 * expansion sound for 5B
 */

static inline void mapper_sun5(uint_fast16_t, uint_fast8_t), sun5_irq();
static uint_fast8_t sun5Command, sun5IrqControl;
static uint_fast16_t sun5IrqCounter;
uint_fast8_t extendedPrg = 0;
void mapper_sun5(uint_fast16_t address, uint_fast8_t value)
{
switch (address & 0xe000)
{
case 0x8000: /* Command Register */
	sun5Command = (value & 0x0f);
	break;
case 0xa000: /* Parameter Register */
	switch (sun5Command)
	{
	case 0x0: /* CHR Bank 0 */
		chrBank[0] = value;
		chr_bank_switch();
		break;
	case 0x1: /* CHR Bank 1 */
		chrBank[1] = value;
		chr_bank_switch();
		break;
	case 0x2: /* CHR Bank 2 */
		chrBank[2] = value;
		chr_bank_switch();
		break;
	case 0x3: /* CHR Bank 3 */
		chrBank[3] = value;
		chr_bank_switch();
		break;
	case 0x4: /* CHR Bank 4 */
		chrBank[4] = value;
		chr_bank_switch();
		break;
	case 0x5: /* CHR Bank 5 */
		chrBank[5] = value;
		chr_bank_switch();
		break;
	case 0x6: /* CHR Bank 6 */
		chrBank[6] = value;
		chr_bank_switch();
		break;
	case 0x7: /* CHR Bank 7 */
		chrBank[7] = value;
		chr_bank_switch();
		break;
	case 0x8: /* PRG Bank 0 */
		if (value & 0x80)
		{
			wramEnable = 1;
			wramSource = cart.bwramSize ? bwram : wram;
		}
		else
			wramEnable = 0;
		if (value & 0x40)
			extendedPrg = 0;
		else
		{
			extendedPrg = 1;
			wramSource = &prg[(((value & 0x3f) & ((cart.prgSize >> 13) - 1)) << 13)];
		}
		break;
	case 0x9: /* PRG Bank 1 */
		prgBank[0] = ((value & 0x3f) << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
		break;
	case 0xa: /* PRG Bank 2 */
		prgBank[2] = ((value & 0x3f) << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
		break;
	case 0xb: /* PRG Bank 3 */
		prgBank[4] = ((value & 0x3f) << 1);
		prgBank[5] = prgBank[4] + 1;
		prg_bank_switch();
		break;
	case 0xc: /* Nametable mirroring */
		switch (value & 0x03)
		{
		case 0:
			cart.mirroring = 1;
			nametable_mirroring(cart.mirroring);
			break;
		case 1:
			cart.mirroring = 0;
			nametable_mirroring(cart.mirroring);
			break;
		case 2:
			cart.mirroring = 2;
			nametable_mirroring(cart.mirroring);
			break;
		case 3:
			cart.mirroring = 3;
			nametable_mirroring(cart.mirroring);
			break;
		}
		break;
	case 0xd: /* IRQ Control */
		sun5IrqControl = value;
		mapperInt = 0;
		break;
	case 0xe: /* IRQ Counter Low */
		sun5IrqCounter = ((sun5IrqCounter & 0xff00) | value);
		break;
	case 0xf: /* IRQ Counter High */
		sun5IrqCounter = ((sun5IrqCounter & 0x00ff) | (value << 8));
		break;
	}
	break;
	case 0xc000: /* Audio register select (5B) */
		printf("Writing %02x to unimplemented register: audio register select 0xc000\n",value);
		break;
	case 0xe000: /* Audio register write (5B) */
		printf("Writing %02x to unimplemented register: audio register write 0xe000\n",value);
		break;
	default:
		printf("Unhandled write of %02x to %04x\n",value,address);
		break;
	break;
}
}

void sun5_irq()
{
	if (sun5IrqControl & 0x80)
	{
		if (((sun5IrqCounter & 0xffff) == 0xffff) && (sun5IrqControl & 0x01))
		{
			mapperInt = 1;
		}
		else sun5IrqCounter--;
	}
}

/*-----------------------------------T*HQ------------------------------------*/

/*/////////////////////////////////*/
/*               CPROM             */
/*/////////////////////////////////*/

static inline void mapper_cprom(uint_fast16_t, uint_fast8_t);

void mapper_cprom(uint_fast16_t address, uint_fast8_t value)
{
	chrBank[4] = ((value & 0x03) << 2);
	chrBank[5] = chrBank[4] + 1;
	chrBank[6] = chrBank[4] + 2;
	chrBank[7] = chrBank[4] + 3;
	chr_bank_switch();
}

/*-----------------------------------TAITO------------------------------------*/

/*/////////////////////////////////*/
/*              TC0190             */
/*/////////////////////////////////*/

/* TODO:
 *
 * IRQ related issues - should be slightly delayed ?
 */

static uint_fast8_t tc0190IrqEnable = 0, tc0190IrqLatch = 0, tc0190IrqReload = 0, tc0190IrqCounter = 0;
static inline void mapper_tc0190(uint_fast16_t, uint_fast8_t);

void mapper_tc0190(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0xe003)
	{
	case 0x8000: /* PRG 0 + mirroring */
		if (!strcmp(cart.slot,"tc0190fmc") || !strcmp(cart.slot,"tc0350fmr"))
		{
		cart.mirroring = 1 - ((value >> 6) & 1);
		nametable_mirroring(cart.mirroring);
		}
		prgBank[0] = ((value & 0x3f) << 1);
		prgBank[1] = prgBank[0] + 1;
		prg_bank_switch();
		break;
	case 0x8001: /* PRG 1 */
		prgBank[2] = ((value & 0x3f) << 1);
		prgBank[3] = prgBank[2] + 1;
		prg_bank_switch();
		break;
	case 0x8002: /* CHR 0 */
		chrBank[0] = (value << 1);
		chrBank[1] = chrBank[0] + 1;
		chr_bank_switch();
		break;
	case 0x8003: /* CHR 1 */
		chrBank[2] = (value << 1);
		chrBank[3] = chrBank[2] + 1;
		chr_bank_switch();
		break;
	case 0xa000: /* CHR 2 */
		chrBank[4] = value;
		chr_bank_switch();
		break;
	case 0xa001: /* CHR 3 */
		chrBank[5] = value;
		chr_bank_switch();
		break;
	case 0xa002: /* CHR 4 */
		chrBank[6] = value;
		chr_bank_switch();
		break;
	case 0xa003: /* CHR 5 */
		chrBank[7] = value;
		chr_bank_switch();
		break;
	case 0xc000: /* IRQ Reload */
		printf("Writing to register c000: %02x\n",value);
		tc0190IrqReload = 1;
		tc0190IrqLatch = (value^0xff);
		break;
	case 0xc001: /* IRQ Clear */
		printf("Writing to register c001: %02x\n",value);
		tc0190IrqCounter = 0;
		break;
	case 0xc002: /* IRQ Enable */
		printf("Writing to register c002: %02x\n",value);
		tc0190IrqEnable = 1;
		break;
	case 0xc003: /* IRQ Acknowledge */
		printf("Writing to register c003: %02x\n",value);
		tc0190IrqEnable = 0;
		mapperInt = 0;
		break;
	case 0xe000: /* PRG 0 + mirroring */
		if (!strcmp(cart.slot,"tc0190fmcp"))
		{
		cart.mirroring = 1 - ((value >> 6) & 1);
		nametable_mirroring(cart.mirroring);
		}
		break;
	default:
		printf("Writing to unmapped register: %04x\n",address & 0xe003);
		break;
	}
}

void tc0190_irq ()
{
	if (tc0190IrqReload || !tc0190IrqCounter)
	{
		tc0190IrqReload = 0;
		tc0190IrqCounter = tc0190IrqLatch;
		if (tc0190IrqEnable && !tc0190IrqCounter)
		{
			mapperInt = 1;
		}
	}
	else if (tc0190IrqCounter > 0)
	{
		tc0190IrqCounter--;
		if (tc0190IrqEnable && !tc0190IrqCounter)
		{
			mapperInt = 1;
		}
	}
}

/*/////////////////////////////////*/
/*              X1-005             */
/*/////////////////////////////////*/

static inline void mapper_x1005(uint_fast16_t, uint_fast8_t);

void mapper_x1005(uint_fast16_t address, uint_fast8_t value)
{
	switch (address & 0x7eff)
	{
	case 0x7ef0: /* CHR Bank 0-1 */
		break;
	case 0x7ef1: /* CHR Bank 2-3 */
		break;
	case 0x7ef2: /* CHR Bank 4 */
		break;
	case 0x7ef3: /* CHR Bank 5 */
		break;
	case 0x7ef4: /* CHR Bank 6 */
		break;
	case 0x7ef5: /* CHR Bank 7 */
		break;
	case 0x7ef6: /* Mirroring */
	case 0x7ef7:
		cart.mirroring = (value & 0x01);
		nametable_mirroring(cart.mirroring);
		break;
	case 0x7ef8:
		break;
	case 0x7ef9:
		break;
	case 0x7efa:
		break;
	case 0x7efb:
		break;
	case 0x7efc:
		break;
	case 0x7efd:
		break;
	case 0x7efe:
		break;
	case 0x7eff:
		break;
	}
}

/*----------------------------------------------------------------------------*/

void reset_default()
{
	if (cart.chrSize)
	{
		chrSource[0] = CHR_ROM;
		chrSource[1] = CHR_ROM;
		chrSource[2] = CHR_ROM;
		chrSource[3] = CHR_ROM;
		chrSource[4] = CHR_ROM;
		chrSource[5] = CHR_ROM;
		chrSource[6] = CHR_ROM;
		chrSource[7] = CHR_ROM;
	}
	else
	{
		chrSource[0] = CHR_RAM;
		chrSource[1] = CHR_RAM;
		chrSource[2] = CHR_RAM;
		chrSource[3] = CHR_RAM;
		chrSource[4] = CHR_RAM;
		chrSource[5] = CHR_RAM;
		chrSource[6] = CHR_RAM;
		chrSource[7] = CHR_RAM;

	}
	prgBank[0] = 0;
	prgBank[1] = 1;
	prgBank[2] = 2;
	prgBank[3] = 3;
	prgBank[4] = cart.pSlots - 4;
	prgBank[5] = cart.pSlots - 3;
	prgBank[6] = cart.pSlots - 2;
	prgBank[7] = cart.pSlots - 1;
	prg_bank_switch();
	chrBank[0] = 0;
	chrBank[1] = 1;
	chrBank[2] = 2;
	chrBank[3] = 3;
	chrBank[4] = 4;
	chrBank[5] = 5;
	chrBank[6] = 6;
	chrBank[7] = 7;
	chr_bank_switch();
	nametable_mirroring(cart.mirroring);
}

void prg_bank_switch()
{
	prgSlot[0] = &prg[((prgBank[0] & (cart.pSlots - 1)) << 12)];
	prgSlot[1] = &prg[((prgBank[1] & (cart.pSlots - 1)) << 12)];
	prgSlot[2] = &prg[((prgBank[2] & (cart.pSlots - 1)) << 12)];
	prgSlot[3] = &prg[((prgBank[3] & (cart.pSlots - 1)) << 12)];
	prgSlot[4] = &prg[((prgBank[4] & (cart.pSlots - 1)) << 12)];
	prgSlot[5] = &prg[((prgBank[5] & (cart.pSlots - 1)) << 12)];
	prgSlot[6] = &prg[((prgBank[6] & (cart.pSlots - 1)) << 12)];
	prgSlot[7] = &prg[((prgBank[7] & (cart.pSlots - 1)) << 12)];
}

void chr_bank_switch()
{
	chrSlot[0] = (chrSource[0] == CHR_RAM) ? &chrRam[((chrBank[0] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[0] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[1] = (chrSource[1] == CHR_RAM) ? &chrRam[((chrBank[1] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[1] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[2] = (chrSource[2] == CHR_RAM) ? &chrRam[((chrBank[2] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[2] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[3] = (chrSource[3] == CHR_RAM) ? &chrRam[((chrBank[3] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[3] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[4] = (chrSource[4] == CHR_RAM) ? &chrRam[((chrBank[4] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[4] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[5] = (chrSource[5] == CHR_RAM) ? &chrRam[((chrBank[5] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[5] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[6] = (chrSource[6] == CHR_RAM) ? &chrRam[((chrBank[6] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[6] & ((cart.chrSize >> 10) - 1)) << 10)];
	chrSlot[7] = (chrSource[7] == CHR_RAM) ? &chrRam[((chrBank[7] & ((cart.cramSize >> 10) - 1)) << 10)] : &chrRom[((chrBank[7] & ((cart.chrSize >> 10) - 1)) << 10)];
}

void nametable_mirroring(uint_fast8_t mode)
{
	switch (mode)
	{
	case 0: /* horizontal */
		nameSlot[0] = ciram;
		nameSlot[1] = ciram;
		nameSlot[2] = ciram + 0x400;
		nameSlot[3] = ciram + 0x400;
		break;
	case 1: /* vertical */
		nameSlot[0] = ciram;
		nameSlot[1] = ciram + 0x400;
		nameSlot[2] = ciram;
		nameSlot[3] = ciram + 0x400;
		break;
	case 2: /* one-page low */
	case 5:
		nameSlot[0] = ciram;
		nameSlot[1] = ciram;
		nameSlot[2] = ciram;
		nameSlot[3] = ciram;
		break;
	case 3: /* one-page high */
		nameSlot[0] = ciram + 0x400;
		nameSlot[1] = ciram + 0x400;
		nameSlot[2] = ciram + 0x400;
		nameSlot[3] = ciram + 0x400;
		break;
	case 4: /* 4-screen, hardwired */
		nameSlot[0] = ciram;
		nameSlot[1] = ciram + 0x400;
		nameSlot[2] = chrRam;
		nameSlot[3] = chrRam + 0x400;
		break;
	}
}

void null_irq() {}
void write_null(uint_fast16_t address, uint_fast8_t value) {}
uint_fast8_t read_null(uint_fast16_t address) {return 0;}

void init_mapper() {
	reset_default();
	irq_cpu_clocked = &null_irq;
	irq_ppu_clocked = &null_irq;
	read_mapper_register = &read_null;
	write_mapper_register4 = &write_null;
	write_mapper_register6 = &write_null;
	write_mapper_register8 = &write_null;
	if(!strcmp(cart.slot,"sxrom") ||
				!strcmp(cart.slot,"sxrom_a") ||
					!strcmp(cart.slot,"sorom") ||
						!strcmp(cart.slot,"sorom_a")) {
		if ((cart.wramSize+cart.bwramSize) && (!strcmp(cart.mmc1_type,"MMC1A") ||
				!strcmp(cart.mmc1_type,"MMC1B1") ||
					!strcmp(cart.mmc1_type,"MMC1B1-H") ||
						!strcmp(cart.mmc1_type,"MMC1B2") ||
							!strcmp(cart.mmc1_type,"MMC1B3"))) {
			wramEnable = 1;
		}
		write_mapper_register8 = &mapper_mmc1;
	} else if(!strcmp(cart.slot,"uxrom") ||
				!strcmp(cart.slot,"un1rom") ||
					!strcmp(cart.slot,"unrom_cc")) {
		write_mapper_register8 = &mapper_uxrom;
	} else if (!strcmp(cart.slot,"cnrom")) {
		write_mapper_register8 = &mapper_cnrom;
	} else if (!strcmp(cart.slot,"axrom")) {
		write_mapper_register8 = &mapper_axrom;
	} else if (!strcmp(cart.slot,"txrom") || !strcmp(cart.slot,"tqrom")
			|| !strcmp(cart.slot,"txsrom")) {
		write_mapper_register8 = &mapper_mmc3;
		irq_ppu_clocked = &mmc3_irq;
		memcpy(&mmc3ChrSource, &chrSource, sizeof(chrSource));
	} else if (!strcmp(cart.slot,"vrc1")) {
		write_mapper_register8 = &mapper_vrc1;
	} else if (!strcmp(cart.slot,"vrc2") ||
			!strcmp(cart.slot,"vrc4")) {
		write_mapper_register8 = &mapper_vrc24;
		if (!strcmp(cart.slot,"vrc2") && (!cart.wramSize && !cart.bwramSize))
			wramBit = 1;
		irq_cpu_clocked = &vrc_irq;
	} else if (!strcmp(cart.slot,"vrc6")) {
		write_mapper_register8 = &mapper_vrc6;
	    expansion_sound = &vrc6_sound;
	    expSound = 1;
	    irq_cpu_clocked = &vrc_irq;
	} else if (!strcmp(cart.slot,"g101")) {
		write_mapper_register8 = &mapper_g101;
	} else if (!strcmp(cart.slot,"lrog017")) {
		write_mapper_register8 = &mapper_lrog017;
		reset_lrog017();
	} else if (!strcmp(cart.slot,"holydivr")) {
			write_mapper_register8 = &mapper_holydivr;
	} else if (!strcmp(cart.slot,"jf16")) {
			write_mapper_register8 = &mapper_jf16;
	} else if (!strcmp(cart.slot,"jf17") || !strcmp(cart.slot,"jf17pcm")) {
		write_mapper_register8 = &mapper_jf17;
	} else if (!strcmp(cart.slot,"namcot_3433") || !strcmp(cart.slot,"namcot_3425")  || !strcmp(cart.slot,"namcot_3446")) {
		write_mapper_register8 = &mapper_namcot34xx;
	} else if (!strcmp(cart.slot,"discrete_74x377")) {
		write_mapper_register8 = &mapper_74x377;
		reset_74x377();
	} else if (!strcmp(cart.slot,"cprom")) {
		write_mapper_register8 = &mapper_cprom;
	} else if (!strcmp(cart.slot,"ss88006")) {
		write_mapper_register8 = &mapper_ss88006;
		irq_cpu_clocked = &ss88006_irq;
	} else if (!strcmp(cart.slot,"namcot_163")) {
		write_mapper_register4 = &mapper_namco163;
		write_mapper_register8 = &mapper_namco163;
		read_mapper_register = &namco163_read;
		irq_cpu_clocked = &namco163_irq;
	} else if (!strcmp(cart.slot,"tc0190fmc") || !strcmp(cart.slot,"tc0190fmcp")
			|| !strcmp(cart.slot,"tc0350fmr")) {
		write_mapper_register8 = &mapper_tc0190;
		irq_ppu_clocked = &tc0190_irq;
	} else if (!strcmp(cart.slot,"bnrom")) {
		write_mapper_register8 = &mapper_bnrom;
	} else if (!strcmp(cart.slot,"nina001")) {
		write_mapper_register6 = &mapper_nina1;
	} else if (!strcmp(cart.slot,"gxrom")) {
		write_mapper_register8 = &mapper_gxrom;
	} else if (!strcmp(cart.slot,"bitcorp_dis")) {
		write_mapper_register6 = &mapper_bitcorp;
		reset_bitcorp();
	} else if (!strcmp(cart.slot,"h3001")) {
		write_mapper_register8 = &mapper_h3001;
		reset_h3001();
		irq_cpu_clocked = &h3001_irq;
	} else if (!strcmp(cart.slot,"sunsoft3")) {
		write_mapper_register8 = &mapper_sun3;
		irq_cpu_clocked = &sun3_irq;
	} else if (!strcmp(cart.slot,"sunsoft4")) {
		write_mapper_register6 = &mapper_sun4;
		write_mapper_register8 = &mapper_sun4;
	} else if (!strcmp(cart.slot,"sunsoft5a") ||
			   !strcmp(cart.slot,"sunsoft5b") ||
			   !strcmp(cart.slot,"sunsoft_fme7")) {
		write_mapper_register8 = &mapper_sun5;
		irq_cpu_clocked = &sun5_irq;
	} else if (!strcmp(cart.slot,"bf9093") || !strcmp(cart.slot,"bf9096")) {
		write_mapper_register8 = &mapper_bf909x;
	} else if (!strcmp(cart.slot,"vrc3")) {
		write_mapper_register8 = &mapper_vrc3;
		irq_cpu_clocked = &vrc3_irq;
	} else if (!strcmp(cart.slot,"nina006")) {
		write_mapper_register4 = &mapper_nina36;
	} else if (!strcmp(cart.slot,"x1_005")) {
		write_mapper_register6 = &mapper_x1005;
	}
}
