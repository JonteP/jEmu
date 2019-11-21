/////////////////////////////////////
//               MMC 1             //
//               SxROM             //
/////////////////////////////////////

/* Emulates the following PCBs:
 *
Board    PRG ROM             PRG RAM  CHR                  Comments

SAROM    64 KB               8 KB     16 / 32 / 64 KB ROM  NES only
SBROM    64 KB                        16 / 32 / 64 KB ROM  NES only
SCROM    64 KB                        128 KB ROM           NES only
SC1ROM   64 KB                        128 KB ROM           Uses 7432 for 28-pin CHR ROM
SEROM    32 KB                        16 / 32 / 64 KB ROM
SFROM    128 / 256 KB                 16 / 32 / 64 KB ROM
SF1ROM   256 KB                       64 KB ROM            PRG uses standard 32-pin EPROM pinout
SFEXPROM 256 KB                       64 KB ROM            Patches PRG at runtime to correct a bad mask ROM run.
SGROM    128 / 256 KB                 8 KB RAM/ROM
SHROM    32 KB                        128 KB ROM           NES only
SH1ROM   32 KB                        128 KB ROM           Uses 7432 for 28-pin CHR ROM
SIROM    32 KB               8 KB     16 / 32 / 64 KB ROM  Japan Only
SJROM    128 / 256 KB        8 KB     16 / 32 / 64 KB ROM
SKROM    128 / 256 KB        8 KB     128 KB ROM
SLROM    128 / 256 KB                 128 KB ROM
SL1ROM   64 / 128 / 256 KB            128 KB ROM           Uses 7432 for 28-pin CHR ROM
SL2ROM   128 / 256 KB                 128 KB ROM           CHR uses standard 32-pin EPROM pinout
SL3ROM   256 KB                       128 KB ROM           Uses 7432 for 28-pin CHR ROM
SLRROM   128 / 256 KB                 128 KB ROM           Difference from SLROM unknown
SMROM    256 KB                       8 KB RAM             Japan Only
SNROM    128 / 256 KB        8 KB     8 KB RAM/ROM
SOROM    128 / 256 KB        16 KB    8 KB RAM/ROM
SUROM    512 KB              8 KB     8 KB RAM/ROM
SXROM    128 / 256 / 512 KB  32 KB    8 KB RAM/ROM         Japan Only
SZROM    128 / 256 KB        16 KB    16-64 KB ROM         Japan Only
 */

/* TODO:
 * -NES-EVENT pcb support (https://wiki.nesdev.com/w/index.php/INES_Mapper_105) - maybe separate implementation?
 * -MMC1C should default to RAM disabled
 *
 * Game specific bugs:
 * -Bart vs Space Mutants - shaking status bar (sprite zero?)
 */

#include "../mapper.h"
#include <string.h>
#include <stdio.h>
#include "../nescartridge.h"
#include "../../video/ppu.h"
#include "../../cpu/6502.h"
#include "../nesemu.h"

static uint8_t shiftCounter;
static uint8_t shiftReg;
static uint8_t controlReg_prgSize;
static uint8_t controlReg_prgSelect;
static uint8_t controlReg_chrSize;
static uint8_t chrReg0;
static uint8_t chrReg1;
static uint8_t prgReg;
static uint8_t prgOffset;
static uint8_t prgRamBank;

static uint16_t chrMask4;
static uint16_t chrMask8;
static uint16_t prgMask16;
static uint16_t prgMask32;
static uint16_t prgRamMask;
static uint8_t prgRamShift;
static uint32_t lastCycle;

static void mmc1_register_write(uint16_t, uint8_t);
static void mmc1_prg_bank_switch();
static void mmc1_chr_bank_switch();

void mmc1_reset() { //TODO: verify startup values
    shiftCounter = 0;
    shiftReg = 0;
    controlReg_prgSize = 0x08;
    controlReg_prgSelect = 0x04;
    controlReg_chrSize = 0;
    cart.mirroring = 0;
    chrReg0 = 0;
    chrReg1 = 0;
    prgReg = 0;
    prgOffset = 0;
    prgRamBank = 0;

    //heuristic to determine PRG RAM banking
    if((cart.bwramSize + cart.wramSize) > 0x2000) {
        if(cart.bwramSize == 0x8000) { //SXROM
            prgRamMask = 0x0c;
            prgRamShift = 11;
        } else if(cart.chrSize > 0x2000) { //SZROM
            prgRamMask = 0x10;
            prgRamShift = 9;
        } else { //SOROM
            prgRamMask = 0x08;
            prgRamShift = 10;
        }
    }
    lastCycle = 0;
    chrMask4 = cart.chrSize ? ((cart.chrSize >> 12) - 1) : ((cart.cramSize >> 12) - 1);
    chrMask8 = (cart.chrSize ? ((cart.chrSize >> 13) - 1) : ((cart.cramSize >> 13) - 1)) << 1;
    if(cart.prgSize == 0x80000) {
        prgMask16 = (cart.prgSize >> 15) - 1;
        prgMask32 = ((cart.prgSize >> 16) - 1) << 1;
    }
    else {
        prgMask16 = (cart.prgSize >> 14) - 1;
        prgMask32 = ((cart.prgSize >> 15) - 1) << 1;
    }
    write_mapper_register = &mmc1_register_write;
    //MMC1A: PRG RAM is always enabled. Two games abuse this lack of feature: they have been allocated to iNES Mapper 155.
    //MMC1B: PRG RAM is enabled by default.
    //MMC1C: PRG RAM is disabled by default. TODO: how to identify these?
}

void mmc1_register_write(uint16_t address, uint8_t value) {
    if (((_6502_M2 - lastCycle) > 1) && (address >= 0x8000)) {
        if (value & 0x80) { //Reset shift register
            shiftCounter = 0;
            shiftReg = 0;
            controlReg_prgSize = 0x08;
            controlReg_prgSelect = 0x04;
        } else {
            shiftReg = (shiftReg & ~(1 << shiftCounter)) | ((value & 1) << shiftCounter); //Write shift register
            if (shiftCounter == 4) {
                switch (address & 0x6000) {
                case 0x0000: //Control register
                    controlReg_prgSize = (shiftReg & 0x08); //0 32k, 1 16k
                    controlReg_prgSelect = (shiftReg & 0x04); //0 low, 1 high
                    controlReg_chrSize = (shiftReg & 0x10); //0 8k,  1 4k
                    switch (shiftReg & 0x03) {
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
                case 0x2000: //CHR ROM low bank
                    chrReg0 = shiftReg;
                    if (cart.prgSize == 0x80000) {
                        prgOffset = (chrReg0 & 0x10);
                    }
                    prgRamBank = chrReg0 & prgRamMask;
                    mmc1_chr_bank_switch();
                    mmc1_prg_bank_switch();
                    break;
                case 0x4000: //CHR ROM high bank (4k mode)
                    chrReg1 = shiftReg;
                    if(controlReg_chrSize) {
                        if (cart.prgSize == 0x80000) {
                            prgOffset = (chrReg0 & 0x10);
                        }
                        prgRamBank = chrReg0 & prgRamMask;
                    }
                    mmc1_chr_bank_switch();
                    mmc1_prg_bank_switch();
                    break;
                case 0x6000: //PRG ROM bank
                    prgReg = shiftReg;
                    if (strcmp(cart.subtype,"MMC1A"))
                        wramEnable = !((prgReg >> 4) & 1);
                    mmc1_prg_bank_switch();
                    break;
                }
                shiftCounter = 0;
                shiftReg = 0;
            } else
                shiftCounter++;
        }
    }
    lastCycle = _6502_M2;
}

void mmc1_prg_bank_switch() {
    if(wramEnable) {
        cpuMemory[0x6]->mask = 0xfff;
        cpuMemory[0x6]->writable = 1;
        cpuMemory[0x6]->memory = wramSource + (prgRamBank << prgRamShift);
        cpuMemory[0x7]->mask = 0xfff;
        cpuMemory[0x7]->writable = 1;
        cpuMemory[0x7]->memory = wramSource + (prgRamBank << prgRamShift) + 0x1000;
    } else {
        cpuMemory[0x6]->mask = 0;
        cpuMemory[0x6]->writable = 0;
        cpuMemory[0x6]->memory = &openBus;
        cpuMemory[0x7]->mask = 0;
        cpuMemory[0x7]->writable = 0;
        cpuMemory[0x7]->memory = &openBus;
    }
    if(controlReg_prgSize) { //16KB PRG banks
        if(controlReg_prgSelect) { //switch 0x8000, fix 0xc000 to last bank
            cpuMemory[0x8]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14);
            cpuMemory[0x9]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x1000;
            cpuMemory[0xa]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x2000;
            cpuMemory[0xb]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x3000;
            cpuMemory[0xc]->memory = prg + ((prgMask16 + prgOffset) << 14);
            cpuMemory[0xd]->memory = prg + ((prgMask16 + prgOffset) << 14) + 0x1000;
            cpuMemory[0xe]->memory = prg + ((prgMask16 + prgOffset) << 14) + 0x2000;
            cpuMemory[0xf]->memory = prg + ((prgMask16 + prgOffset) << 14) + 0x3000;
        } else if(!controlReg_prgSelect) { //switch 0xc000, fix 0x8000 to first bank
            cpuMemory[0x8]->memory = prg + (prgOffset << 14);
            cpuMemory[0x9]->memory = prg + (prgOffset << 14) + 0x1000;
            cpuMemory[0xa]->memory = prg + (prgOffset << 14) + 0x2000;
            cpuMemory[0xb]->memory = prg + (prgOffset << 14) + 0x3000;
            cpuMemory[0xc]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14);
            cpuMemory[0xd]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x1000;
            cpuMemory[0xe]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x2000;
            cpuMemory[0xf]->memory = prg + (((prgReg & prgMask16) + prgOffset) << 14) + 0x3000;
        }
    }
    else if(!controlReg_prgSize) { //32KB PRG bank
        cpuMemory[0x8]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14);
        cpuMemory[0x9]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x1000;
        cpuMemory[0xa]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x2000;
        cpuMemory[0xb]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x3000;
        cpuMemory[0xc]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x4000;
        cpuMemory[0xd]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x5000;
        cpuMemory[0xe]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x6000;
        cpuMemory[0xf]->memory = prg + (((prgReg & prgMask32) + prgOffset) << 14) + 0x7000;
    }
}

void mmc1_chr_bank_switch() {
    if (controlReg_chrSize) { //4k banks
        chrSlot[0] = ((chrSource[0] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask4) << 12)) : (chrRom + ((chrReg0 & chrMask4) << 12)));
        chrSlot[1] = ((chrSource[1] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask4) << 12)) : (chrRom + ((chrReg0 & chrMask4) << 12))) + 0x400;
        chrSlot[2] = ((chrSource[2] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask4) << 12)) : (chrRom + ((chrReg0 & chrMask4) << 12))) + 0x800;
        chrSlot[3] = ((chrSource[3] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask4) << 12)) : (chrRom + ((chrReg0 & chrMask4) << 12))) + 0xc00;
        chrSlot[4] = ((chrSource[4] == CHR_RAM) ? (chrRam + ((chrReg1 & chrMask4) << 12)) : (chrRom + ((chrReg1 & chrMask4) << 12)));
        chrSlot[5] = ((chrSource[5] == CHR_RAM) ? (chrRam + ((chrReg1 & chrMask4) << 12)) : (chrRom + ((chrReg1 & chrMask4) << 12))) + 0x400;
        chrSlot[6] = ((chrSource[6] == CHR_RAM) ? (chrRam + ((chrReg1 & chrMask4) << 12)) : (chrRom + ((chrReg1 & chrMask4) << 12))) + 0x800;
        chrSlot[7] = ((chrSource[7] == CHR_RAM) ? (chrRam + ((chrReg1 & chrMask4) << 12)) : (chrRom + ((chrReg1 & chrMask4) << 12))) + 0xc00;
    } else if (!controlReg_chrSize) { //8k bank
        chrSlot[0] = ((chrSource[0] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12)));
        chrSlot[1] = ((chrSource[1] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x0400;
        chrSlot[2] = ((chrSource[2] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x0800;
        chrSlot[3] = ((chrSource[3] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x0c00;
        chrSlot[4] = ((chrSource[4] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x1000;
        chrSlot[5] = ((chrSource[5] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x1400;
        chrSlot[6] = ((chrSource[6] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x1800;
        chrSlot[7] = ((chrSource[7] == CHR_RAM) ? (chrRam + ((chrReg0 & chrMask8) << 12)) : (chrRom + ((chrReg0 & chrMask8) << 12))) + 0x1c00;
    }
}
