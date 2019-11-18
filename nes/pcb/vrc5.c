/////////////////////////////////////
//              VRC 5 +            //
//            KONAMI-QT            //
/////////////////////////////////////

#include "../mapper.h"
#include "../../video/ppu.h"
#include "../nescartridge.h"
#include "../nesemu.h"

static uint8_t qtRam[0x800];

static uint8_t vrc5_irqControl;
static uint16_t vrc5_irqLatch;
static uint16_t vrc5_irqCounter;
static uint8_t vrc5_tilePosition;
static uint8_t vrc5_tileAttribute;
static uint8_t vrc5_column;
static uint8_t vrc5_row;
static uint8_t ciramByte;
static uint8_t qtramByte;
static uint8_t qtVal;
static uint8_t bank8;
static uint8_t bankA;
static uint8_t bankC;
static uint8_t ntTarget;

static void vrc5_register_write(uint16_t, uint8_t);
static uint8_t vrc5_register_read(uint16_t);
static uint8_t* vrc5_ppu_read_chr(uint16_t);
static uint8_t* vrc5_ppu_read_nt(uint16_t);
static void vrc5_ppu_write_nt(uint16_t, uint8_t);
static void vrc5_irq(void);
static void kanji_decoder();

void vrc5_reset() {
    write_mapper_register = &vrc5_register_write;
    read_mapper_register = &vrc5_register_read;
    ppu_read_chr = &vrc5_ppu_read_chr;
    ppu_read_nt = &vrc5_ppu_read_nt;
    ppu_write_nt = &vrc5_ppu_write_nt;
    irq_cpu_clocked = &vrc5_irq;
}

void vrc5_register_write(uint16_t address, uint8_t value) {
    switch(address & 0xff00) {
    case 0xd000: //WRAM Bank Select, 0x6000
        cpuMemory[0x6]->memory = ((value & 0x08) ? wram : bwram) + ((value & 0x01) << 12);
        break;
    case 0xd100: //WRAM Bank Select, 0x7000
        cpuMemory[0x7]->memory = ((value & 0x08) ? wram : bwram) + ((value & 0x01) << 12);
        break;
    case 0xd200: //PRG-ROM Bank Select, 0x8000
        bank8 = ((value & 0x40) ? ((value & 0x3f) + 0x10) : (value & 0x0f));
        cpuMemory[0x8]->memory = prg + (bank8 << 13);
        cpuMemory[0x9]->memory = prg + (bank8 << 13) + 0x1000;
        break;
    case 0xd300: //PRG-ROM Bank Select, 0xa000
        bankA = ((value & 0x40) ? ((value & 0x3f) + 0x10) : (value & 0x0f));
        cpuMemory[0xa]->memory = prg + (bankA << 13);
        cpuMemory[0xb]->memory = prg + (bankA << 13) + 0x1000;
        break;
    case 0xd400: //PRG-ROM Bank Select, 0xc000
        bankC = ((value & 0x40) ? ((value & 0x3f) + 0x10) : (value & 0x0f));
        cpuMemory[0xc]->memory = prg + (bankC << 13);
        cpuMemory[0xd]->memory = prg + (bankC << 13) + 0x1000;
        break;
    case 0xd500: //CHR-RAM Bank Select
        chrSlot[0] = chrRam + ((value & 0x01) << 12);
        chrSlot[1] = chrRam + ((value & 0x01) << 12) + 0x400;
        chrSlot[2] = chrRam + ((value & 0x01) << 12) + 0x800;
        chrSlot[3] = chrRam + ((value & 0x01) << 12) + 0xc00;
        break;
    case 0xd600: //IRQ Latch Write, LSB
        vrc5_irqLatch = (vrc5_irqLatch & 0xff00) | value;
        break;
    case 0xd700: //IRQ Latch Write, MSB
        vrc5_irqLatch = (vrc5_irqLatch & 0x00ff) | (value << 8);
        break;
    case 0xd800: //IRQ Acknowledge
        vrc5_irqControl = (vrc5_irqControl << 1) | (vrc5_irqControl & 0x01);
        mapperInt = 0;
        break;
    case 0xd900: //IRQ Control
        vrc5_irqControl = value & 0x03;
        if(vrc5_irqControl & 0x02)
            vrc5_irqCounter = vrc5_irqLatch;
        mapperInt = 0;
        break;
    case 0xda00: //Nametable Control
        cart.mirroring = ((value & 0x02) ? H_MIRROR : V_MIRROR);
        nametable_mirroring(cart.mirroring);
        ntTarget = value & 0x01;
        break;
    case 0xdb00:
        vrc5_tilePosition = value & 0x03;
        vrc5_tileAttribute = value & 0x04;
        break;
    case 0xdc00: //Character Translation Output, tile
        vrc5_column = value;
        break;
    case 0xdd00: //Character Translation Output, bank
        vrc5_row = value;
        break;
    default:
        break;
    }
}

uint8_t vrc5_register_read(uint16_t address) {
    switch(address & 0xff00) {
    case 0xdc00:
        kanji_decoder();
        mapperRead = 1;
        return ciramByte;
    case 0xdd00:
        kanji_decoder();
        mapperRead = 1;
        return qtramByte;
    default:
        mapperRead = 0;
        return 0;
    }
}

uint8_t retVal;
uint8_t* vrc5_ppu_read_chr(uint16_t address) {
    if(address & 0x1000) { //BG tile fetch
        if(qtVal & 0x40) { //QTa Kanji CHR ROM
            if(address & 0x08) {
                retVal = (qtVal & 0x80) ? 0xff : 0x00;
                return &retVal;
            }
            else
                return &chrRom[((qtVal & 0x3f) << 12) | (address & 0xfff)];
        }
        else  //BG tile from external CHR RAM
            return &chrRam[((qtVal & 0x01) << 12) | (address & 0xfff)];
    }
    else //Sprite tile from external CHR RAM
        return &chrSlot[(address >> 10)][address & 0x3ff];
}

uint8_t* vrc5_ppu_read_nt(uint16_t address) {
    if((address & 0x3ff) < 0x3c0) {//exclude reads from attribute table
        qtVal = qtRam[(cart.mirroring ? (address & 0x400) : ((address & 0x800) >> 1)) | (address & 0x3ff)];
    }
    return &nameSlot[(address >> 10) & 0x03][address & 0x3ff];
}

void vrc5_ppu_write_nt(uint16_t address, uint8_t value) {
    if(ntTarget)
        qtRam[(cart.mirroring ? (address & 0x400) : ((address & 0x800) >> 1)) | (address & 0x3ff)] = value;
    else
        nameSlot[(address >> 10) & 3][address & 0x3ff] = value;
}

static const uint8_t conv_tbl[4][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x40, 0x10, 0x28, 0x00, 0x18, 0x30 },
    { 0x00, 0x00, 0x48, 0x18, 0x30, 0x08, 0x20, 0x38 },
    { 0x00, 0x00, 0x80, 0x20, 0x38, 0x10, 0x28, 0xb0 }
};

void kanji_decoder() {
    uint8_t tabl = conv_tbl[(vrc5_column >> 5) & 3][(vrc5_row & 0x7f) >> 4];
    qtramByte = 0x40 | (tabl & 0x3f) | ((vrc5_row >> 1) & 7) | (vrc5_tileAttribute << 5);
    ciramByte = ((vrc5_row & 0x01) << 7) | ((vrc5_column & 0x1f) << 2) | vrc5_tilePosition;
    if(tabl & 0x40)
        qtramByte &= 0xfb;
    else if(tabl & 0x80)
        qtramByte |= 0x04;
}

void vrc5_irq() {
    if(vrc5_irqControl & 0x02) {
        if(!vrc5_irqCounter) {
            mapperInt = 1;
            vrc5_irqCounter = vrc5_irqLatch;
        }
        else
            vrc5_irqCounter++;
    }
}
