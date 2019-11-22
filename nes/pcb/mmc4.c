/////////////////////////////////////
//               MMC 4             //
//               FxROM             //
/////////////////////////////////////

/* Emulates the following PCBs:

Board   PRG ROM         PRG RAM     CHR
FJROM   128 / 256 KB    8 KB        16 / 32 / 64 KB ROM
FKROM   128 / 256 KB    8 KB        128 KB ROM
*/

#include "../mapper.h"
#include <string.h>
#include <stdio.h>
#include "../nescartridge.h"
#include "../../video/ppu.h"
#include "../../cpu/6502.h"
#include "../nesemu.h"

static uint8_t  prgReg;
static uint8_t chrReg0;
static uint8_t chrReg1;
static uint8_t chrReg2;
static uint8_t chrReg3;
static uint16_t prgMask;
static uint16_t chrMask;
static uint16_t latch0;
static uint16_t latch1;

static void mmc4_register_write(uint16_t, uint8_t);
static void mmc4_prg_mapping();
static void mmc4_chr_mapping();
static uint8_t* mmc4_ppu_read_chr(uint16_t);

void mmc4_reset() { //TODO: correct startup values
    latch0 = 0;
    latch1 = 0;
    prgMask = (cart.prgSize >> 14) - 1;
    chrMask = (cart.chrSize >> 12) - 1;
    mmc4_prg_mapping();
    write_mapper_register = &mmc4_register_write;
    ppu_read_chr = &mmc4_ppu_read_chr;
}

void mmc4_register_write(uint16_t address, uint8_t value) {
    switch (address & 0xf000) {
    case 0xa000: //PRG ROM bank select
        prgReg = value & 0xf;
        mmc4_prg_mapping();
        break;
    case 0xb000: //CHR ROM bank select, low 1
        chrReg0 = value;
        mmc4_chr_mapping();
        break;
    case 0xc000: //CHR ROM bank select, low 2
        chrReg1 = value;
        mmc4_chr_mapping();
        break;
    case 0xd000: //CHR ROM bank select, high 1
        chrReg2 = value;
        mmc4_chr_mapping();
        break;
    case 0xe000: //CHR ROM bank select, high 2
        chrReg3 = value;
        mmc4_chr_mapping();
        break;
    case 0xf000: //Mirroring
        cart.mirroring = 1 - (value & 0x01);
        nametable_mirroring(cart.mirroring);
        break;
    }
}

uint8_t* mmc4_ppu_read_chr(uint16_t address) {
    uint8_t *pattern = &chrSlot[(address >> 10)][address & 0x3ff];
    if(address >= 0x0fd8 && address <=0x0fdf)
        latch0 = 0;
    else if(address >= 0x0fe8 && address <=0x0fef)
        latch0 = 1;
    if(address >= 0x1fd8 && address <=0x1fdf)
        latch1 = 0;
    else if(address >= 0x1fe8 && address <=0x1fef)
        latch1 = 1;
    mmc4_chr_mapping();
    return pattern;
}

void mmc4_prg_mapping() {
    cpuMemory[0x6]->memory = wramSource;
    cpuMemory[0x7]->memory = wramSource + 0x1000;
    cpuMemory[0x8]->memory = prg + ((prgReg & prgMask) << 14);
    cpuMemory[0x9]->memory = prg + ((prgReg & prgMask) << 14) + 0x1000;
    cpuMemory[0xa]->memory = prg + ((prgReg & prgMask) << 14) + 0x2000;
    cpuMemory[0xb]->memory = prg + ((prgReg & prgMask) << 14) + 0x3000;
    cpuMemory[0xc]->memory = prg + ( prgMask           << 14);
    cpuMemory[0xd]->memory = prg + ( prgMask           << 14) + 0x1000;
    cpuMemory[0xe]->memory = prg + ( prgMask           << 14) + 0x2000;
    cpuMemory[0xf]->memory = prg + ( prgMask           << 14) + 0x3000;
}

void mmc4_chr_mapping() {
    chrSlot[0] = chrRom + (((latch0 ? chrReg1 : chrReg0) & chrMask) << 12);
    chrSlot[1] = chrRom + (((latch0 ? chrReg1 : chrReg0) & chrMask) << 12) + 0x400;
    chrSlot[2] = chrRom + (((latch0 ? chrReg1 : chrReg0) & chrMask) << 12) + 0x800;
    chrSlot[3] = chrRom + (((latch0 ? chrReg1 : chrReg0) & chrMask) << 12) + 0xc00;
    chrSlot[4] = chrRom + (((latch1 ? chrReg3 : chrReg2) & chrMask) << 12);
    chrSlot[5] = chrRom + (((latch1 ? chrReg3 : chrReg2) & chrMask) << 12) + 0x400;
    chrSlot[6] = chrRom + (((latch1 ? chrReg3 : chrReg2) & chrMask) << 12) + 0x800;
    chrSlot[7] = chrRom + (((latch1 ? chrReg3 : chrReg2) & chrMask) << 12) + 0xc00;
}
