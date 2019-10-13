/* Famicom Disk System
 * consists of the RAM Adaptor and disk drive
 *
 * RAM Adaptor
 *     32KB DRAM: PRG RAM mapped to 6502 @ 0x6000-0xdfff
 *      8KB  RAM: CHR RAM mapped to PPU  @ 0x0000-0x1fff
 *      RP2C33(A) ASIC:
 *          8KB ROM: BIOS mapped to 6502 @ 0xe000-0xffff
 *          Disk controller
 *          Wavetable synthesizer
 *
 * Disk drive
 *     disks: based on the Mitsumi Quick Disk format
 *            2 sided; ~16KB storage capacity per side
 *
 * Versions:
 * -Twin Famicom features a built in FDS
 *
 * TODO:
 * -expansion sound emulation
 * -auto eject/insert function when switching side or disk
 */

#include "fds.h"
#include <stdio.h>
#include <stdint.h>
#include <linux/limits.h>
#include "nescartridge.h"
#include "../jemu.h"
#include "mapper.h"
#include "nesemu.h"
#include "../cpu/6502.h"

#define BIOS_SIZE	        0x2000
#define DISK_SIDE_SIZE      65500 //as per the .fds format
#define FDS_HEADER_SIZE     16 //as per the .fds format
#define DISK_HEADER_SIZE    56
#define FILE_COUNT_SIZE     2
#define FILE_HEADER_SIZE    16
#define PREGAP              28300   //variable gap size on real HW
#define INTERFILE_GAP       976
#define GAPEND              0x80
#define CRC1                0x4d    //where does this come from?
#define CRC2                0x62    //TODO: replace with crc calculation

static const uint8_t fdsHeaderString[4] = {0x46, 0x44, 0x53, 0x1a};//Identifying string for .fds format (16b) header

//RP2C33 register flags:
static uint16_t irqReload;      //4020.x - 4021.x
static uint8_t irqRepeat;       //4022.0
static uint8_t irqEnabled;      //4022.1
static uint8_t enableSoundReg;  //4023.1
static uint8_t writeData;       //4024.x (byte written to disk)
static uint8_t motorOn;         //4025.0
static uint8_t resetTransfer;   //4025.1
static uint8_t readMode;        //4025.2
static uint8_t crcControl;      //4025.4
static uint8_t diskReady;       //4025.6
static uint8_t diskIrqEnabled;  //4025.7
static uint8_t transferFlag;    //4030.1
static uint8_t endOfHead;       //4030.6
static uint8_t enableDiskReg;   //4030.7
static uint8_t readData;        //4031.x (byte read from disk)
       uint8_t diskFlag;        //4032.0
static uint8_t readyFlag;       //4032.1
static uint8_t protectFlag;     //4032.2
static uint8_t extOutput;       //4026.x / 4033.x

static uint8_t gapEnded;        //Gap end bit (non-zero == 0x80) encountered

static uint8_t fdsHeader[FDS_HEADER_SIZE];
       FILE *biosFile;
       FILE *diskFile;
       char bios[PATH_MAX];
       char disk[PATH_MAX];
       uint8_t fdsRam[0x8000];         //mapped to 6502: 0x6000-0xdfff
       uint8_t *fdsBiosRom = NULL;     //mapped to 6502: 0xe000-0xffff
static uint8_t *tmpDisk = NULL;        //disk image in .fds format
static uint8_t *diskData = NULL;       //reconstructed disk image
       uint8_t currentDiskSide;
static uint8_t numSides;
static uint8_t diskInt;
static uint16_t irqCounter;         //decremented by 1 each cpu clock if IRQ enabled
static uint16_t diskPosition;       //current position of header on disk
static uint32_t delay;              //simulates seek time of disk drive

static void zero_fill(uint8_t *, int);
static void reconstruct_disk(uint8_t *, uint8_t *);

void fds_load_disk(char *disk) {
	sprintf(bios, "bios/%s",currentMachine->bios);
	if ((biosFile = fopen(bios, "r")) == NULL) {
		printf("Error: No such file\n");
		exit(EXIT_FAILURE);
	}
	free(fdsBiosRom);
	fdsBiosRom = malloc(BIOS_SIZE * sizeof(uint8_t));
	fread(fdsBiosRom, BIOS_SIZE, 1, biosFile);
	strcpy(cart.slot, "fds");
	cart.prgSize = 0;
	cart.cramSize = 8192;
	free(chrRam);

	currentDiskSide = 0;
	chrRam = malloc(cart.cramSize * sizeof(uint8_t));
    if ((diskFile = fopen(disk, "r")) == NULL) {
        diskFlag = 0x01;
    }
    fread(fdsHeader, sizeof(fdsHeader), 1, diskFile);
    uint8_t headerless = 0;
    for (int i = 0; i < sizeof(fdsHeaderString); i++) {
        if (fdsHeader[i] != fdsHeaderString[i]) {
            headerless = 1;
        }
    }
    if(headerless) {
        fseek(diskFile, 0, SEEK_END);
        uint32_t fSize = ftell(diskFile);
        numSides = fSize / DISK_SIDE_SIZE;
        fseek(diskFile, 0, SEEK_SET);
    } else
        numSides = fdsHeader[4];

    tmpDisk = malloc(DISK_SIDE_SIZE * numSides * sizeof(uint8_t));
    fread(tmpDisk, DISK_SIDE_SIZE * numSides, 1, diskFile);
    fclose(diskFile);

    free(diskData);
    diskData = malloc(((DISK_SIDE_SIZE * numSides) << 1) * sizeof(uint8_t)); // make sure there's a margin
    for (int i = 0; i < numSides; i++) {
        reconstruct_disk(tmpDisk + i * DISK_SIDE_SIZE, diskData + ((i * DISK_SIDE_SIZE) << 1));
    }
    free(tmpDisk);
}

void reconstruct_disk(uint8_t *source, uint8_t *dest) {
//parse disk image and insert gap and crc data
    uint16_t filePosition = 0;
    uint8_t blockType;
    uint16_t blockLength;
    zero_fill(dest, PREGAP >> 3);
    filePosition += (PREGAP >> 3);
    for (int i = 0; i < DISK_SIDE_SIZE;) {
        blockType = source[i];
        switch(blockType) {
        case 1:
            blockLength = DISK_HEADER_SIZE;
            break;
        case 2:
            blockLength = FILE_COUNT_SIZE;
            break;
        case 3:
            blockLength = FILE_HEADER_SIZE;
            break;
        case 4:
            blockLength = (1 + source[i - 3] + (source[i - 2] << 8));
            break;
        default:
            return;
        }
        dest[filePosition++] = GAPEND;
        for(int j = 0; j < blockLength; j++) {
            dest[filePosition++] = source[i + j];
        }
        dest[filePosition++] = CRC1;
        dest[filePosition++] = CRC2;
        zero_fill(dest + filePosition, INTERFILE_GAP >> 3);
        filePosition += (INTERFILE_GAP >> 3);
        i += blockLength;
    }
}

uint8_t read_fds_register(uint16_t address) {
	switch (address) {
	case 0x4030: ;//FDS: Disk Status Register 0
	    uint8_t ret = ((enableDiskReg ? 0x80 : 0x00) |
	                       (endOfHead ? 0x40 : 0x00) |
	                     //  (crcFail ? 0x10 : 0x00) |
	                    (transferFlag ? 0x02 : 0x00) |
	                                       mapperInt);
        mapperInt = 0;//both
        diskInt = 0;
	    transferFlag = 0;
	    return ret;
	case 0x4031://FDS: Read data register
	    transferFlag = 0;
        diskInt = 0;//disk
        return readData;
	case 0x4032://FDS: Disk drive status register
		return (protectFlag | (readyFlag ? 0x02 : 0x00) | diskFlag); //ready flag is checked after motor is turned on
	case 0x4033://FDS: External connector read
	    return (0x80 | (extOutput & 0x7f));
		break;
	}
	return 0;
}

void write_fds_register(uint16_t address, uint8_t value) {
	switch(address) {
	case 0x4020: //FDS: IRQ reload value low
		irqReload = ((irqReload & 0xff00) | value);
		break;
	case 0x4021: //FDS: IRQ reload value high
	    irqReload = ((irqReload & 0x00ff) | (value << 8));
		break;
	case 0x4022: //FDS: IRQ control
	    irqEnabled = enableDiskReg ? (value & 0x02) : 0;
        irqRepeat = value & 0x01;
        if(irqEnabled) {
            irqCounter = irqReload;
        } else
            mapperInt = 0;
		break;
	case 0x4023: //FDS: Master I/O enable
		enableDiskReg = (value & 0x01);
        enableSoundReg = (value & 0x02);
        if(!enableDiskReg) {
            irqEnabled = 0;
            mapperInt = 0;
            diskInt = 0;
        }
		break;
	case 0x4024: //FDS: Write data register
	    writeData = value;
	    transferFlag = 0;
	    diskInt = 0;
		break;
	case 0x4025: //FDS: Control
        diskInt = 0;
		diskIrqEnabled  =  value & 0x80;
        diskReady       =  value & 0x40;
		crcControl      =  value & 0x10;
        cart.mirroring  = (value & 0x08) ? H_MIRROR : V_MIRROR;
		readMode        =  value & 0x04;
		resetTransfer   =  value & 0x02;
		motorOn         =  value & 0x01;

		nametable_mirroring(cart.mirroring);
		break;
	case 0x4026: //FDS: External connector
		extOutput = (value & 0x7f);
		break;
	}
}

void run_fds(uint16_t ntimes) {
    while(ntimes) {
        if(irqEnabled) {
            if(!irqCounter) {
                mapperInt = 1;
                irqCounter = irqReload;
                if(!irqRepeat)
                    irqEnabled = 0;
            }
            else
                irqCounter--;
        }

        uint8_t isIrq = diskIrqEnabled;
        uint8_t tmpData = 0;
        uint8_t fdsDisabled = 0;

        if(!motorOn) {
            endOfHead = 1;
            readyFlag = 1;
            fdsDisabled = 1;
        }
        if(resetTransfer && readyFlag) {
            fdsDisabled = 1;
        }
        if(!fdsDisabled) {
            if(endOfHead) {
                delay = 50000;
                gapEnded = 0;
                endOfHead = 0;
                diskPosition = 0;
            }
            if(delay > 0)
                delay--;
            else {
                readyFlag = 0;
                if(readMode) {
                    tmpData = diskData[diskPosition + ((currentDiskSide * DISK_SIDE_SIZE) << 1)];
                    if(!diskReady) {
                        gapEnded = 0;
                    }else if(tmpData && !gapEnded) {
                        gapEnded = 1;
                        isIrq = 0;
                    }
                    if(gapEnded) {
                        transferFlag = 1;
                        readData = tmpData;
                        if(isIrq) {
                            diskInt = 1;//disk
                        }
                    }
                } else {
                  if(!crcControl) {
                      transferFlag = 1;
                      tmpData = writeData;
                      if(isIrq)
                          diskInt = 1;//disk
                  }
                  if(!diskReady)
                      tmpData = 0;
                  diskData[diskPosition + ((currentDiskSide * DISK_SIDE_SIZE) << 1)] = tmpData;
                }

                diskPosition++;
                if(diskPosition >= DISK_SIDE_SIZE) {
                    motorOn = 0;
                } else
                    delay = 150;
            }
            //TODO: all interrupts should be checked in nesemu.c
            if (diskInt && !irqPulled) {
                irqPulled = 1;
            }
        }
        ntimes--;
        fds_wait--;
    }
}

void zero_fill(uint8_t *dest, int count) {
    for(int i = 0; i < count; i++) {
        *(dest + i) = 0;
    }
}

void init_fds(void) {
    irqReload = 0;
    irqRepeat = 0;
    irqEnabled = 0;
    enableSoundReg = 0;
    motorOn = 0;
    resetTransfer = 0;
    readMode = 0;
    crcControl = 0;
    diskReady = 0;
    diskIrqEnabled = 0;
    transferFlag = 0;
    endOfHead = 0;
    enableDiskReg = 0;
    readData = 0;
    diskFlag = 0;
    readyFlag = 0;
    protectFlag = 0;
    gapEnded = 0;
    currentDiskSide = 0;
    numSides = 0;
    diskInt = 0;
    irqCounter = 0;
    diskPosition = 0;
    delay = 0;
}
