/////////////////////////////////////
//              MMC 3              //
//              TxROM              //
/////////////////////////////////////

/* Emulates the following PCBs:

Board   PRG ROM             PRG RAM             CHR                                                     Comments

TBROM   64 KB                                   16 / 32 / 64 KB ROM
TEROM   32 KB                                   16 / 32 / 64 KB ROM                                     Supports fixed mirroring
TFROM   128 / 256 / 512 KB                      16 / 32 / 64 KB ROM                                     Supports fixed mirroring
TGROM   128 / 256 / 512 KB                      8 KB RAM/ROM
TKROM   128 / 256 / 512 KB  8 KB                128 / 256 KB ROM
TK1ROM  128 KB              8 KB                128KB ROM                                               Uses 7432 for 28-pin CHR ROM
TKSROM  128 / 256 / 512 KB  8 KB                128 KB ROM                                              Alternate mirroring control, Famicom only
TLROM   128 / 256 / 512 KB                      128 / 256 KB ROM
TL1ROM  128 KB                                  128 KB                                                  Uses 7432 for 28-pin CHR ROM
TL2ROM                                                                                                  Nonstandard pinout
TLBROM  128 KB                                  128 KB ROM                                              Uses 74541 to compensate for too-slow CHR ROM
TLSROM  128 / 256 / 512 KB                      128 KB ROM                                              Alternate mirroring control
TNROM   128 / 256, 512 KB   8 KB                8 KB RAM/ROM                                            Famicom only
TQROM   128 KB                                  16 / 32 / 64 KB ROM + 8 KB RAM
TR1ROM  128 / 256 / 512 KB                      64 KB ROM + 4 KB VRAM (4-screen Mirroring)              NES only
TSROM   128 / 256 / 512 KB  8 KB (no battery)   128 / 256 KB ROM
TVROM   64 KB                                   16 / 32 / 64 KB ROM + 4 KB VRAM (4-screen Mirroring)    NES only
 */


/* TODO:
 * Proper IRQ timing (see relevant test ROM; hacked in a delay for now)
 * Support for mapper 47 etc. (many mmc3 derivatives exist)
 */

#include "../mapper.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../../video/ppu.h"
#include "../nescartridge.h"
#include "../nesemu.h"

static uint8_t bank8;
static uint8_t bankA;
static uint8_t bankC;
static uint8_t bankE;
static uint16_t cramMask;
static uint16_t cromMask;

static uint8_t bankSelect;
static uint8_t bankRegister[0x08];
static uint8_t irqEnable;
static uint8_t irqLatch;
static uint8_t irqReload;
static uint8_t irqCounter;
static chrtype_t mmc3ChrSource[0x8];
static uint16_t lastAddress;
static int32_t lastCycle;
static uint8_t downCycles;

static void     mmc3_register_write(uint16_t, uint8_t);
static void     mmc3_prg_bank_switch();
static void     mmc3_chr_bank_switch();
static void     mmc3_irq_old(void);
static void     mmc3_irq_new(void);
static void     (*mmc3_irq)(void);
static uint8_t* mmc3_ppu_read_chr(uint16_t);


void mmc3_reset() { //TODO: verify startup values
    write_mapper_register = &mmc3_register_write;
    ppu_read_chr = &mmc3_ppu_read_chr;
    if(!strcmp(cart.subtype,"MMC3A"))//TODO: are some MMC3B using old behavior?
        mmc3_irq = &mmc3_irq_old;
    else
        mmc3_irq = &mmc3_irq_new;
    memcpy(&mmc3ChrSource, &chrSource, sizeof(chrSource));
    lastAddress = 0;
    lastCycle = 0;
    cramMask = ((cart.cramSize >> 10) - 1);
    cromMask = ((cart.chrSize >> 10) - 1);
}

void mmc3_register_write (uint16_t address, uint8_t value) {
    switch (address & 0xe000) {
    case 0x8000:
        if (!(address % 2)) { //Bank select (0x8000)
            bankSelect = value;
            mmc3_chr_bank_switch();
            mmc3_prg_bank_switch();

        } else { //Bank data (0x8001)
            int bank = (bankSelect & 0x07);
            bankRegister[bank] = value;
            if (bank < 6) {
                if (!strcmp(cart.slot,"tqrom"))
                    mmc3ChrSource[bank] = ((value & 0x40) ? CHR_RAM : CHR_ROM);
                mmc3_chr_bank_switch();
            } else
                mmc3_prg_bank_switch();
        }
        break;
    case 0xa000:
        if (!(address % 2) && strcmp(cart.slot,"txsrom")) { //Mirroring (0xA000)
            cart.mirroring = 1 - (value & 0x01);
            nametable_mirroring(cart.mirroring);
        } else if (address % 2) { //PRG RAM protect (0xA001)
            if(wramSource != NULL) {
                cpuMemory[0x6]->mask = ((value & 0x80) ? 0xfff : 0);
                cpuMemory[0x6]->writable = ((value & 0x40) ? 0 : 1);
                cpuMemory[0x6]->memory = ((value & 0x80) ? wramSource : &openBus);
                cpuMemory[0x7]->mask = ((value & 0x80) ? 0xfff : 0);
                cpuMemory[0x7]->writable = ((value & 0x40) ? 0 : 1);
                cpuMemory[0x7]->memory = ((value & 0x80) ? wramSource + 0x1000 : &openBus);
            }
        }
        break;
    case 0xc000:
        if (!(address % 2)) { //IRQ latch (0xC000)
            irqLatch = value;
        } else { //IRQ reload (0xC001)
            irqReload = 1;
            irqCounter = 0;
        }
        break;
    case 0xe000:
        if (!(address % 2)) { //IRQ disable and acknowledge (0xe000)
            irqEnable = 0;
            mapperInt = 0;
        } else { //IRQ enable (0xe001)
            irqEnable = 1;
        }
        break;
    }
}

void mmc3_prg_bank_switch() {
    if (bankSelect & 0x40) {
        bank8 = ((cart.prgSize) >> 13) - 2;
        bankC = bankRegister[6];
    } else {
        bank8 = bankRegister[6];
        bankC = ((cart.prgSize) >> 13) - 2;
    }
    bankA = bankRegister[7];
    bankE = ((cart.prgSize) >> 13) - 1;

    cpuMemory[0x8]->memory = prg + (bank8 << 13);
    cpuMemory[0x9]->memory = prg + (bank8 << 13) + 0x1000;
    cpuMemory[0xa]->memory = prg + (bankA << 13);
    cpuMemory[0xb]->memory = prg + (bankA << 13) + 0x1000;
    cpuMemory[0xc]->memory = prg + (bankC << 13);
    cpuMemory[0xd]->memory = prg + (bankC << 13) + 0x1000;
    cpuMemory[0xe]->memory = prg + (bankE << 13);
    cpuMemory[0xf]->memory = prg + (bankE << 13) + 0x1000;
}

void mmc3_chr_bank_switch() {
    if(bankSelect & 0x80) {
        if(!strcmp(cart.slot,"txsrom")) {
            nameSlot[0] = (bankRegister[2] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[1] = (bankRegister[3] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[2] = (bankRegister[4] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[3] = (bankRegister[5] & 0x80) ? ciRam : (ciRam + 0x400);
        }
        chrSource[0] = mmc3ChrSource[2];
        chrSource[1] = mmc3ChrSource[3];
        chrSource[2] = mmc3ChrSource[4];
        chrSource[3] = mmc3ChrSource[5];
        chrSource[4] = mmc3ChrSource[0];
        chrSource[5] = mmc3ChrSource[0];
        chrSource[6] = mmc3ChrSource[1];
        chrSource[7] = mmc3ChrSource[1];
        chrSlot[0] = (chrSource[0] == CHR_RAM) ? &chrRam[ (bankRegister[2]         & cramMask) << 10] : &chrRom[ (bankRegister[2]         & cromMask) << 10];
        chrSlot[1] = (chrSource[1] == CHR_RAM) ? &chrRam[ (bankRegister[3]         & cramMask) << 10] : &chrRom[ (bankRegister[3]         & cromMask) << 10];
        chrSlot[2] = (chrSource[2] == CHR_RAM) ? &chrRam[ (bankRegister[4]         & cramMask) << 10] : &chrRom[ (bankRegister[4]         & cromMask) << 10];
        chrSlot[3] = (chrSource[3] == CHR_RAM) ? &chrRam[ (bankRegister[5]         & cramMask) << 10] : &chrRom[ (bankRegister[5]         & cromMask) << 10];
        chrSlot[4] = (chrSource[4] == CHR_RAM) ? &chrRam[((bankRegister[0] & 0xfe) & cramMask) << 10] : &chrRom[((bankRegister[0] & 0xfe) & cromMask) << 10];
        chrSlot[5] = (chrSource[5] == CHR_RAM) ? &chrRam[((bankRegister[0] | 0x01) & cramMask) << 10] : &chrRom[((bankRegister[0] | 0x01) & cromMask) << 10];
        chrSlot[6] = (chrSource[6] == CHR_RAM) ? &chrRam[((bankRegister[1] & 0xfe) & cramMask) << 10] : &chrRom[((bankRegister[1] & 0xfe) & cromMask) << 10];
        chrSlot[7] = (chrSource[7] == CHR_RAM) ? &chrRam[((bankRegister[1] | 0x01) & cramMask) << 10] : &chrRom[((bankRegister[1] | 0x01) & cromMask) << 10];
    }
    else if(!(bankSelect & 0x80)) {
        if(!strcmp(cart.slot,"txsrom")) {
            nameSlot[0] = (bankRegister[0] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[1] = (bankRegister[0] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[2] = (bankRegister[1] & 0x80) ? ciRam : (ciRam + 0x400);
            nameSlot[3] = (bankRegister[1] & 0x80) ? ciRam : (ciRam + 0x400);
        }
        chrSource[0] = mmc3ChrSource[0];
        chrSource[1] = mmc3ChrSource[0];
        chrSource[2] = mmc3ChrSource[1];
        chrSource[3] = mmc3ChrSource[1];
        chrSource[4] = mmc3ChrSource[2];
        chrSource[5] = mmc3ChrSource[3];
        chrSource[6] = mmc3ChrSource[4];
        chrSource[7] = mmc3ChrSource[5];
        chrSlot[0] = (chrSource[0] == CHR_RAM) ? &chrRam[((bankRegister[0] & 0xfe) & cramMask) << 10] : &chrRom[((bankRegister[0] & 0xfe) & cromMask) << 10];
        chrSlot[1] = (chrSource[1] == CHR_RAM) ? &chrRam[((bankRegister[0] | 0x01) & cramMask) << 10] : &chrRom[((bankRegister[0] | 0x01) & cromMask) << 10];
        chrSlot[2] = (chrSource[2] == CHR_RAM) ? &chrRam[((bankRegister[1] & 0xfe) & cramMask) << 10] : &chrRom[((bankRegister[1] & 0xfe) & cromMask) << 10];
        chrSlot[3] = (chrSource[3] == CHR_RAM) ? &chrRam[((bankRegister[1] | 0x01) & cramMask) << 10] : &chrRom[((bankRegister[1] | 0x01) & cromMask) << 10];
        chrSlot[4] = (chrSource[4] == CHR_RAM) ? &chrRam[ (bankRegister[2]         & cramMask) << 10] : &chrRom[ (bankRegister[2]         & cromMask) << 10];
        chrSlot[5] = (chrSource[5] == CHR_RAM) ? &chrRam[ (bankRegister[3]         & cramMask) << 10] : &chrRom[ (bankRegister[3]         & cromMask) << 10];
        chrSlot[6] = (chrSource[6] == CHR_RAM) ? &chrRam[ (bankRegister[4]         & cramMask) << 10] : &chrRom[ (bankRegister[4]         & cromMask) << 10];
        chrSlot[7] = (chrSource[7] == CHR_RAM) ? &chrRam[ (bankRegister[5]         & cramMask) << 10] : &chrRom[ (bankRegister[5]         & cromMask) << 10];
    }
}

//TODO: should mmc3 be clocked on ppu writes as well?
uint8_t irqNext = 0;
uint8_t* mmc3_ppu_read_chr(uint16_t address) {
    if(address < 0x3f00) {
        if(irqNext) {
            irqNext = 0;
            mmc3_irq();
        }
    if((address ^ lastAddress) & 0x1000) { //A12 change
       if(address & 0x1000) { //rising edge
           if(lastCycle > ppucc)
                  downCycles = (89342 - lastCycle) + ppucc;
           else
                  downCycles = (ppucc - lastCycle);
           if(downCycles >= 6)
               irqNext = 1;//TODO: hacked in a delay for rockman 5
               //mmc3_irq();
       }
       else if(!(address & 0x1000)) //falling edge
           lastCycle = ppucc;
    }
    lastAddress = address;
    }
    return &chrSlot[(address >> 10)][address & 0x3ff];
}

void mmc3_irq_old() {
    uint8_t oldCounter = irqCounter;
    if(!irqCounter || irqReload)
        irqCounter = irqLatch;
    else
        irqCounter--;
    if((oldCounter || irqReload) && !irqCounter && irqEnable)
        mapperInt = 1;
    irqReload = 0;
}

void mmc3_irq_new() {
    if(!irqCounter || irqReload)
        irqCounter = irqLatch;
    else
        irqCounter--;
    if(!irqCounter && irqEnable)
        mapperInt = 1;
    irqReload = 0;
}
