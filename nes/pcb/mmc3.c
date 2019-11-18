/////////////////////////////////////
//              MMC 3              //
//              TxROM              //
/////////////////////////////////////

/* Emulates:
 *
 * TBROM
 * TEROM
 * TFROM
 * TGROM
 * TKROM
 * TK1ROM
 * TKSROM
 * TLROM
 * TL1ROM
 * TL2ROM
 * TLBROM
 * TLSROM
 * TNROM
 * TQROM    supports simultaneous CHR RAM and ROM
 * TR1ROM
 * TSROM
 * TVROM
 */


/* TODO:
 * better A12 listening?
 * Support for mapper 47 etc.
 * IRQ issues:
 * -Ninja ryuukenden 2 - cut scenes
 * -Rockman3 - possible one-line glitch in weapons screen
 * -Rockman5 - slight glitch in gyromans elevators and level start screen
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
static uint8_t pramProtect;
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


void mmc3_reset() {
    write_mapper_register = &mmc3_register_write;
    //ppu_read_chr = &mmc3_ppu_read_chr;
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
                if (!strcmp(cart.slot,"tqrom")) {
                    mmc3ChrSource[bank] = ((value & 0x40) ? CHR_RAM : CHR_ROM);
                }
                //printf("CHR bank: %02x\t%i\n",bankSelect,ppu_vCounter);
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
            pramProtect = value;
        }
        break;
    case 0xc000:
        if (!(address % 2)) { //IRQ latch (0xC000)
            irqLatch = value;
            //printf("IRQ latch(%i): %i\t%i\n",irqLatch,ppu_vCounter,frame);
        } else if (address % 2) { //IRQ reload (0xC001)
            irqReload = 1;
            irqCounter = 0;
        }
        break;
    case 0xe000:
        if (!(address % 2)) { //IRQ disable and acknowledge (0xe000)
            //printf("IRQ ack: %i\t%i\n",ppu_vCounter,frame);
            irqEnable = 0;
            mapperInt = 0;
        } else if (address % 2) { //IRQ enable (0xe001)
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
// && ((ppucc - lastCycle) >= 6)
//
void mmc3_ppu_read_chr(uint16_t address) {
    if(address < 0x3f00) {
    if((address ^ lastAddress) & 0x1000) { //A12 change
       if(address & 0x1000) { //rising edge
           if(lastCycle > ppucc)
                  downCycles = (89342 - lastCycle) + ppucc;
           else
                  downCycles = (ppucc - lastCycle);
           if(downCycles >= 6)
               mmc3_irq();
       }
       else if(!(address & 0x1000)) //falling edge
           lastCycle = ppucc;
    }
    lastAddress = address;
  //  return &chrSlot[(address >> 10)][address & 0x3ff];
    }
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
    if(!irqCounter || irqReload) {
        irqCounter = irqLatch;
        //printf("IRQ reload (%i): %i\t%i\n",irqLatch,ppu_vCounter,frame);
    }
    else
        irqCounter--;
    if(!irqCounter && irqEnable) {
        mapperInt = 1;
        //printf("IRQ set: %i\t%i\n",ppu_vCounter,frame);
    }
    irqReload = 0;
}
