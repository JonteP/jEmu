/* Console versions:
 * Family Computer (Famicom) (Japan) - July 15, 1983
 *
 * The export front loading versions of the console (NES) uses a CIC lockout chip. A few different variants exist:
 * 3193a; 6113 - both found in US consoles; SM590 4-bit microcontroller with 508 byte internal ROM
 */

#include "nesemu.h"
#include <assert.h>
#include <stdint.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit; free */
#include <string.h>	/* memcpy */
#include <unistd.h>
#include "../video/ppu.h"
#include "../audio/apu.h"
#include "../cpu/6502.h"
#include "../my_sdl.h"
#include "mapper.h"
#include "fds.h"
#include "nescartridge.h"
#include "../jemu.h"

#define FRAC_BITS			16

/* TODO:
 * -better sync handling, needs to read ppu one cycle earlier
 * -does not reset properly when loading game?
 * -VS system
 * -PC-10
 *
 * Game specific issues:
 * -Battle toads - freezes at random on 2nd level (sprite zero related)
 */

float fps;
uint8_t ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0;
uint8_t openBus;
static uint32_t ppu_wait = 0;
static uint32_t apu_wait = 0;
static uint32_t fds_wait = 0;
uint32_t ppuClockRatio;
FILE *logfile;
char *romName;

/* Mapped memory */
uint8_t *prgSlot[0x08], cpuRam[0x800], ppuRegs[0x08], apuRegs[0x20];
struct memSlot *cpuMemory[0x10] = {NULL}, *ppuMemory[0x08] = {NULL}, defaultSlot = {0, 0, NULL};

//                          MACHINE              BIOS       CART        MASTER CLOCK		VIDEO		REGION		VIDEO CARD		AUDIO CARD		HAS EXPANSION SOUND
struct machine nes_ntsc = {     NES,             NULL,        "",    NES_NTSC_MASTER,        NTSC,      EXPORT,       PPU_NTSC,       APU_NTSC,                       0 },
                nes_pal = {     NES,             NULL,        "",     NES_PAL_MASTER,         PAL,      EXPORT,        PPU_PAL,        APU_PAL,                       0 },
                famicom = {     NES,             NULL,        "",    NES_NTSC_MASTER,        NTSC,       JAPAN,       PPU_NTSC,       APU_NTSC,                       1 },
                    fds = {     NES,    "disksys.rom",        "",    NES_NTSC_MASTER,        NTSC,       JAPAN,       PPU_NTSC,       APU_NTSC,                       1 };

static void nes_p1b1(uint8_t), nes_p1b2(uint8_t), nes_p1start(uint8_t),
        nes_p1up(uint8_t), nes_p1down(uint8_t), nes_p1left(uint8_t),
        nes_p1right(uint8_t), nes_p1select(uint8_t);
static void nes_6502_addcycles(uint8_t), nes_6502_synchronize(int),
        nes_reset_emulation(void), init_video(), init_audio(), set_timings(),
        nes_6502_cpuwrite(uint16_t, uint8_t), write_cpu_register(uint16_t, uint8_t);
static uint8_t nes_6502_cpuread(uint16_t), read_cpu_register(uint16_t);

int nesemu() {

    //hook up general
    reset_emulation = &nes_reset_emulation;

    //map mermory
    for(int i = 0; i < 0x10; i++) {
        free(cpuMemory[i]);
        cpuMemory[i] = malloc(sizeof(cpuMemory));
        *cpuMemory[i] = defaultSlot;
    }
    cpuMemory[0x0]->mask = 0x7ff;
    cpuMemory[0x0]->writable = 1;
    cpuMemory[0x0]->memory = cpuRam;
    cpuMemory[0x1]->mask = 0x7ff;
    cpuMemory[0x1]->writable = 1;
    cpuMemory[0x1]->memory = cpuRam;
    cpuMemory[0x2]->mask = 0x7;
    cpuMemory[0x2]->writable = 1;
    cpuMemory[0x2]->memory = ppuRegs;
    cpuMemory[0x3]->mask = 0x7;
    cpuMemory[0x3]->writable = 1;
    cpuMemory[0x3]->memory = ppuRegs;
    cpuMemory[0x4]->mask = 0x1f;
    cpuMemory[0x4]->writable = 1;
    cpuMemory[0x4]->memory = apuRegs;
    cpuMemory[0x5]->mask = 0;
    cpuMemory[0x5]->writable = 0;
    cpuMemory[0x5]->memory = &openBus;
    cpuMemory[0x6]->mask = 0;
    cpuMemory[0x6]->writable = 0;
    cpuMemory[0x6]->memory = &openBus;
    cpuMemory[0x7]->mask = 0;
    cpuMemory[0x7]->writable = 0;
    cpuMemory[0x7]->memory = &openBus;

    //hook up cpu function
    _6502_cpuread = &nes_6502_cpuread;
    _6502_cpuwrite = &nes_6502_cpuwrite;
    _6502_synchronize = &nes_6502_synchronize;
    _6502_addcycles = &nes_6502_addcycles;

    //Hook up input functions
    player1_button1 = &nes_p1b1;
    player1_button2 = &nes_p1b2;
    player1_buttonUp = &nes_p1up;
    player1_buttonDown = &nes_p1down;
    player1_buttonLeft = &nes_p1left;
    player1_buttonRight = &nes_p1right;
    player1_buttonStart = &nes_p1start;
    player1_buttonSelect = &nes_p1select;

    strcpy(currentMachine->cartFile,
            "/home/jonas/git/roms/nes/mmc1/metroid.nes");

    nes_reset_emulation();

    while (quit == 0) {
        run_6502();
        if (stateLoad) {
            stateLoad = 0;
            load_state();
        } else if (stateSave) {
            stateSave = 0;
            save_state();
        }
    }

    //fclose(logfile);
    nes_close_rom();
    return 0;
}

void init_audio() {
    settings.audioFrequency = 48000;
    settings.channels = 1;
    settings.audioBufferSize = 2048;
    init_apu(settings.audioBufferSize);
    init_sdl_audio();
    if (currentMachine->videoSystem == NTSC) {
        set_timings_apu(NTSC_APU_CLOCK_DIV * settings.audioFrequency,
                currentMachine->masterClock);
    } else if (currentMachine->videoSystem == PAL) {
        set_timings_apu(PAL_APU_CLOCK_DIV * settings.audioFrequency,
                currentMachine->masterClock);
    }
}

void init_video() {
    init_audio();
    if (currentMachine->videoSystem == NTSC) {
        ppuCurrentMode = &ntscMode;
        //	fps = (float)NES_NTSC_MASTER/((((DOTS_PER_FRAME * 2) - 1) / 2) * NTSC_PPU_CLOCK_DIV);
        fps = (float) currentMachine->masterClock
                / (ppuCurrentMode->scanlines * DOTS_PER_SCANLINE
                        * NTSC_PPU_CLOCK_DIV);
        ppuClockRatio = (NTSC_CPU_CLOCK_DIV << FRAC_BITS) / NTSC_PPU_CLOCK_DIV;
    } else if (currentMachine->videoSystem == PAL) {
        ppuCurrentMode = &palMode;
        fps = (float) currentMachine->masterClock
                / (ppuCurrentMode->scanlines * DOTS_PER_SCANLINE
                        * PAL_PPU_CLOCK_DIV);
        ppuClockRatio = (PAL_CPU_CLOCK_DIV << FRAC_BITS) / PAL_PPU_CLOCK_DIV;
    }
    settings.window.name = "smsEmu";
    settings.window.screenHeight = ppuCurrentMode->height;
    settings.window.screenWidth = ppuCurrentMode->width;
    settings.window.xClip = 0;
    settings.window.yClip = 0;
    init_ppu();
    init_sdl_video();
}

void set_timings() {
    frameTime = (float) ((1 / fps) * 1000000000);
    init_time(frameTime);
}

void nes_reset_emulation() {
    init_audio();
    init_video();
    set_timings();
    if (currentMachine->bios != NULL) {
        init_fds();
        fds_load_disk(currentMachine->cartFile);
    }
    else
        nes_load_rom(currentMachine->cartFile);
    init_mapper();
    _6502_power_reset(HARD_RESET);
}

/* TODO:
 * -save mirroring mode
 * -save state between cpu instructions
 * -save slot banks
 */

void save_state() {
    char *stateName = strdup(romName);
    sprintf(stateName + strlen(stateName) - 3, "sta");
    FILE *stateFile = fopen(stateName, "w");
//    fwrite(prgBank, sizeof(prgBank), 1, stateFile);
    fwrite(cpuRam, sizeof(cpuRam), 1, stateFile);
/*    fwrite(&cpuA, sizeof(cpuA), 1, stateFile);
    fwrite(&cpuX, sizeof(cpuX), 1, stateFile);
    fwrite(&cpuY, sizeof(cpuY), 1, stateFile);
    fwrite(&cpuP, sizeof(cpuP), 1, stateFile);
    fwrite(&cpuS, sizeof(cpuS), 1, stateFile);
    fwrite(&cpuPC, sizeof(cpuPC), 1, stateFile);*/
 //   fwrite(chrBank, sizeof(chrBank), 1, stateFile);
    fwrite(chrSource, sizeof(chrSource), 1, stateFile);
    fwrite(oam, sizeof(oam), 1, stateFile);
    fwrite(nameSlot, sizeof(nameSlot), 1, stateFile);
    fwrite(ciRam, sizeof(ciRam), 1, stateFile);
    fwrite(palette, sizeof(palette), 1, stateFile);
/*    fwrite(&ppudot, sizeof(ppudot), 1, stateFile);
    fwrite(&ppu_vCounter, sizeof(ppu_vCounter), 1, stateFile);*/
/*    fwrite(&ppuW, sizeof(ppuW), 1, stateFile);
    fwrite(&ppuX, sizeof(ppuX), 1, stateFile);
    fwrite(&ppuT, sizeof(ppuT), 1, stateFile);
    fwrite(&ppuV, sizeof(ppuV), 1, stateFile);*/
    fwrite(&cart.mirroring, sizeof(cart.mirroring), 1, stateFile);
    if (cart.wramSize)
        fwrite(wram, cart.wramSize, 1, stateFile);
    else if (cart.bwramSize)
        fwrite(bwram, cart.bwramSize, 1, stateFile);
    if (cart.cramSize)
        fwrite(chrRam, cart.cramSize, 1, stateFile);
    fclose(stateFile);
}

void load_state() {
    char *stateName = strdup(romName);
    sprintf(stateName + strlen(stateName) - 3, "sta");
    FILE *stateFile = fopen(stateName, "r");
    /* TODO: check for existing save */
    int readErr = 0;
//    readErr |= fread(prgBank, sizeof(prgBank), 1, stateFile);
    readErr |= fread(cpuRam, sizeof(cpuRam), 1, stateFile);
/*    readErr |= fread(&cpuA, sizeof(cpuA), 1, stateFile);
    readErr |= fread(&cpuX, sizeof(cpuX), 1, stateFile);
    readErr |= fread(&cpuY, sizeof(cpuY), 1, stateFile);
    readErr |= fread(&cpuP, sizeof(cpuP), 1, stateFile);
    readErr |= fread(&cpuS, sizeof(cpuS), 1, stateFile);
    readErr |= fread(&cpuPC, sizeof(cpuPC), 1, stateFile);*/
//    readErr |= fread(chrBank, sizeof(chrBank), 1, stateFile);
    readErr |= fread(chrSource, sizeof(chrSource), 1, stateFile);
    readErr |= fread(oam, sizeof(oam), 1, stateFile);
    readErr |= fread(nameSlot, sizeof(nameSlot), 1, stateFile);
    readErr |= fread(ciRam, sizeof(ciRam), 1, stateFile);
    readErr |= fread(palette, sizeof(palette), 1, stateFile);
/*    readErr |= fread(&ppudot, sizeof(ppudot), 1, stateFile);
    readErr |= fread(&ppu_vCounter, sizeof(ppu_vCounter), 1, stateFile);*/
/*    readErr |= fread(&ppuW, sizeof(ppuW), 1, stateFile);
    readErr |= fread(&ppuX, sizeof(ppuX), 1, stateFile);
    readErr |= fread(&ppuT, sizeof(ppuT), 1, stateFile);
    readErr |= fread(&ppuV, sizeof(ppuV), 1, stateFile);*/
    readErr |= fread(&cart.mirroring, sizeof(cart.mirroring), 1, stateFile);
    if (cart.wramSize)
        readErr |= fread(wram, cart.wramSize, 1, stateFile);
    else if (cart.bwramSize)
        readErr |= fread(bwram, cart.bwramSize, 1, stateFile);
    if (cart.cramSize)
        readErr |= fread(chrRam, cart.cramSize, 1, stateFile);
    fclose(stateFile);
    prg_bank_switch();
    chr_bank_switch();
}

//6502 functions

uint8_t s = 0; //TODO: may need to restore related function

uint8_t nes_6502_cpuread(uint16_t address) {
    openBus = address >> 4; //TODO: correct emulation involves preserving last value read by 6502
 /*   if (address >= 0x6000 && address < 0x8000) {
        if (wramEnable || extendedPrg) {
            return wramSource[(address & 0x1fff)];
        } else if (wramBit) {
            return (((address >> 4) & 0xfe) | wramBitVal);
        }
        else
            return (address >> 4); //open bus
    } */
    if (address >= 0x2000 && address < 0x4000)
        return read_ppu_register(address);
    else if (address >= 0x4000 && address < 0x4020)
        return read_cpu_register(address);
    else if (address >= 0x4030 && address < 0x4040 && currentMachine->bios != NULL)
        return read_fds_register(address);
    uint8_t mapperVal = read_mapper_register(address);
    if(mapperRead) {
        mapperRead = 0;
        return mapperVal;
    }
 /*   else {
        return (address >> 4); //open bus
    }*/
    struct memSlot *mem = cpuMemory[address >> 12];
    assert(mem->memory != NULL);
    return mem->memory[address & mem->mask];
}

void nes_6502_cpuwrite(uint16_t address, uint8_t value) {
    struct memSlot *mem = cpuMemory[address >> 12];
    assert(mem->memory != NULL);
    if(mem->writable)
        mem->memory[address & mem->mask] = value;
    if (address >= 0x2000 && address < 0x4000)
        write_ppu_register(address, value);
    else if (address >= 0x4000 && address < 0x4020)
        write_cpu_register(address, value);
    else if (address >= 0x4020 && address < 0x4030 && currentMachine->bios != NULL)
        write_fds_register(address, value);
    else if (address >= 0x4030)
  /*      if (wramEnable && !extendedPrg)
            wramSource[(address & 0x1fff)] = value;
        else if (wramBit) //used by VRC2/4
            wramBitVal = (value & 0x01); */
        write_mapper_register(address, value);
}

//TODO: is this machine implementation specific?

uint8_t read_cpu_register(uint16_t address) {
    uint8_t value;
    switch (address) {
    case 0x4015: /* APU status read */
        value = (dmcInt ? 0x80 : 0) | (frameInt ? 0x40 : 0)
                | ((dmcBytesLeft) ? 0x10 : 0) | (noiseLength ? 0x08 : 0)
                | (triLength ? 0x04 : 0) | (pulse2Length ? 0x02 : 0)
                | (pulse1Length ? 0x01 : 0);
        /* TODO: timing related inhibition of frameInt clear */
        frameInt = 0;
        return value;
    case 0x4016: /* TODO: proper open bus emulation */
        value = ((ctr1 >> ctrb) & 1) | 0x40;
        if (s == 0)
            ctrb++;
        return value;
    case 0x4017:
        value = ((ctr2 >> ctrb2) & 1) | 0x40;
        if (s == 0)
            ctrb2++;
        return value;
    }
    return 0;
}

void write_cpu_register(uint16_t address, uint8_t value) {
    uint16_t source;
    switch (address) {
    case 0x4000: /* Pulse 1 duty, envel., volume */
        pulse1Control = value;
        env1Divide = (pulse1Control & 0xf);
        break;
    case 0x4001: /* Pulse 1 sweep, period, negate, shift */
        sweep1 = value;
        sweep1Divide = ((sweep1 >> 4) & 7);
        sweep1Shift = (sweep1 & 7);
        sweep1Reload = 1;
        break;
    case 0x4002: /* Pulse 1 timer low */
        pulse1Timer = (pulse1Timer & 0x700) | value;
        break;
    case 0x4003: /* Pulse 1 length counter, timer high */
        if (apuStatus & 1)
            pulse1Length = lengthTable[((value >> 3) & 0x1f)];
        pulse1Timer = (pulse1Timer & 0xff) | ((value & 7) << 8);
        env1Start = 1;
        pulse1Duty = 0;
        break;
    case 0x4004: /* Pulse 2 duty, envel., volume */
        pulse2Control = value;
        env2Divide = (pulse2Control & 0xf);
        break;
    case 0x4005: /* Pulse 2 sweep, period, negate, shift */
        sweep2 = value;
        sweep2Divide = ((sweep2 >> 4) & 7);
        sweep2Shift = (sweep2 & 7);
        sweep2Reload = 1;
        break;
    case 0x4006: /* Pulse 2 timer low */
        pulse2Timer = (pulse2Timer & 0x700) | value;
        break;
    case 0x4007: /* Pulse 2 length counter, timer high */
        if (apuStatus & 2)
            pulse2Length = lengthTable[((value >> 3) & 0x1f)];
        pulse2Timer = (pulse2Timer & 0xff) | ((value & 7) << 8);
        env2Start = 1;
        pulse2Duty = 0;
        break;
    case 0x4008: /* Triangle misc. */
        triControl = value;
        break;
    case 0x400a: /* Triangle timer low */
        triTimer = (triTimer & 0x700) | value;
        break;
    case 0x400b: /* Triangle length, timer high */
        if (apuStatus & 4) {
            triLinReload = 1;
            triLength = lengthTable[((value >> 3) & 0x1f)];
        }
        triTimer = (triTimer & 0xff) | ((value & 7) << 8);
        break;
    case 0x400c: /* Noise misc. */
        noiseControl = value;
        envNoiseDivide = (noiseControl & 0xf);
        break;
    case 0x400e: /* Noise loop, period */
        noiseTimer = noiseTable[(value & 0xf)];
        noiseMode = (value & 0x80);
        break;
    case 0x400f: /* Noise length counter */
        if (apuStatus & 8) {
            noiseLength = lengthTable[((value >> 3) & 0x1f)];
            envNoiseStart = 1;
        }
        break;
    case 0x4010: /* DMC IRQ, loop, freq. */
        dmcControl = value;
        dmcRate = dmcRateTable[currentMachine->audioCard][(dmcControl & 0xf)];
        dmcTemp = dmcRate;
        if (!(dmcControl & 0x80)) {
            dmcInt = 0;
        }
        break;
    case 0x4011: /* DMC load counter */
        dmcOutput = (value & 0x7f);
        break;
    case 0x4012: /* DMC sample address */
        dmcAddress = (0xc000 + (value << 6));
        dmcCurAdd = dmcAddress;
        break;
    case 0x4013: /* DMC sample length */
        dmcLength = ((value << 4) + 1);
        break;
    case 0x4014:
        source = (value << 8);
        if (_6502_M2 % 2)
            _6502_addcycles(2);
        else
            _6502_addcycles(1);
        _6502_synchronize(0);
        for (int i = 0; i < 256; i++) {
            if (ppuOamAddress > 255)
                ppuOamAddress = 0;
            oam[ppuOamAddress++] = _6502_cpuread(source++);
            _6502_addcycles(2);
            _6502_synchronize(0);
        }
        break;
    case 0x4015: /* APU status */
        dmcInt = 0;
        apuStatus = value;
        if (!(apuStatus & 0x01))
            pulse1Length = 0;
        if (!(apuStatus & 0x02))
            pulse2Length = 0;
        if (!(apuStatus & 0x04))
            triLength = 0;
        if (!(apuStatus & 0x08))
            noiseLength = 0;
        if (!(apuStatus & 0x10)) {
            dmcBytesLeft = 0;
            dmcSilence = 1;
        } else if (apuStatus & 0x10) {
            if (!dmcBytesLeft)
                dmcRestart = 1;
        }
        break;
    case 0x4016:
        s = (value & 1);
        if (s == 1) {
            ctrb = 0;
            ctrb2 = 0;
        }
        break;
    case 0x4017: /* APU frame counter */
        frameWrite = 1;
        frameWriteDelay = 2 + (apucc % 2);
        apuFrameCounter = value;
        break;
    }
}

void nes_6502_addcycles(uint8_t val) {
    ppu_wait += (val * ppuClockRatio);
    apu_wait += val;
    fds_wait += val;
    _6502_M2 += val;
}

void nes_6502_synchronize(int x) {
    run_ppu((ppu_wait - (x * ppuClockRatio)) >> FRAC_BITS);
    run_apu(apu_wait - x);
    run_fds(fds_wait - x);
    ppu_wait = (x * ppuClockRatio);
    apu_wait = x;
    fds_wait = x;
    if (ppu_drawFrame) {
        ppu_drawFrame = 0;
        render_frame(ppuScreenBuffer);
    }
}

//Controller functions
void nes_p1b2(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 0);
}
void nes_p1b1(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 1);
}
void nes_p1select(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 2);
}
void nes_p1start(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 3);
}
void nes_p1up(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 4);
}
void nes_p1down(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 5);
}
void nes_p1left(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 6);
}
void nes_p1right(uint8_t buttonDown) {
    bitset(&ctr1, buttonDown, 7);
}
