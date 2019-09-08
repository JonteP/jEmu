#include "globals.h"
#include <stdint.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>	/* malloc; exit; free */
#include <string.h>	/* memcpy */
#include <unistd.h>
#include "ppu.h"
#include "apu.h"
#include "6502.h"
#include "cartridge.h"
#include "my_sdl.h"
#include "mapper.h"

/* TODO:
 *
 * -FIR low pass filtering of sound
 * -fix sound buffering (if broken)
 * -better sync handling, needs to read ppu one cycle earlier
 *
 * Features:
 * -file load routines
 * -oam viewer
 * -save states
 */

const float originalFps = 60.098823055, originalCpuClock = 1789773, cyclesPerFrame = 29780.5;
float fps, cpuClock;
uint_fast8_t quit = 0, ctrb = 0, ctrb2 = 0, ctr1 = 0, ctr2 = 0;
uint_fast8_t openBus;
uint16_t vdp_wait = 0, apu_wait = 0;
FILE *logfile;
char *romName;

int nesemu(int argc, char *argv[]) {

	fps = originalFps;
	cpuClock = originalCpuClock;

	if (argc < 2)
		romName = "/home/jonas/games/nes/mappers/mmc3/rockman3.nes";
	else
		romName = argv[1];
	load_rom(romName);

	logfile = fopen("/home/jonas/eclipse-workspace/logfile.txt","w");
	if (logfile==NULL)
		printf("Error: Could not create logfile\n");

	rstFlag = HARD_RESET;
	init_sdl();
	init_time();

	while (quit == 0)
	{
		opdecode();
		synchronize(0);
		if (stateLoad)
		{
			stateLoad = 0;
			load_state();
		}
		else if (stateSave)
		{
			stateSave = 0;
			save_state();
		}
	}

	fclose(logfile);
	close_rom();
	close_sdl();
}

/* TODO:
 * -save mirroring mode
 * -save state between cpu instructions
 * -save slot banks
 */

void save_state()
{
	char *stateName = strdup(romName);
	sprintf(stateName+strlen(stateName)-3,"sta");
	FILE *stateFile = fopen(stateName, "w");
	fwrite(prgBank,sizeof(prgBank),1,stateFile);
	fwrite(cpuRam,sizeof(cpuRam),1,stateFile);
	fwrite(&cpuA,sizeof(cpuA),1,stateFile);
	fwrite(&cpuX,sizeof(cpuX),1,stateFile);
	fwrite(&cpuY,sizeof(cpuY),1,stateFile);
	fwrite(&cpuP,sizeof(cpuP),1,stateFile);
	fwrite(&cpuS,sizeof(cpuS),1,stateFile);
	fwrite(&cpuPC,sizeof(cpuPC),1,stateFile);
	fwrite(chrBank,sizeof(chrBank),1,stateFile);
	fwrite(chrSource,sizeof(chrSource),1,stateFile);
	fwrite(oam,sizeof(oam),1,stateFile);
	fwrite(nameSlot,sizeof(nameSlot),1,stateFile);
	fwrite(ciram,sizeof(ciram),1,stateFile);
	fwrite(palette,sizeof(palette),1,stateFile);
	fwrite(&ppudot,sizeof(ppudot),1,stateFile);
	fwrite(&vCounter,sizeof(vCounter),1,stateFile);
	fwrite(&ppuW,sizeof(ppuW),1,stateFile);
	fwrite(&ppuX,sizeof(ppuX),1,stateFile);
	fwrite(&ppuT,sizeof(ppuT),1,stateFile);
	fwrite(&ppuV,sizeof(ppuV),1,stateFile);
	fwrite(&cart.mirroring,sizeof(cart.mirroring),1,stateFile);
	if (cart.wramSize)
		fwrite(wram,cart.wramSize,1,stateFile);
	else if (cart.bwramSize)
		fwrite(bwram,cart.bwramSize,1,stateFile);
	if (cart.cramSize)
		fwrite(chrRam,cart.cramSize,1,stateFile);
	fclose(stateFile);
}

void load_state()
{
	char *stateName = strdup(romName);
	sprintf(stateName+strlen(stateName)-3,"sta");
	FILE *stateFile = fopen(stateName, "r");
	/* TODO: check for existing save */
	int readErr = 0;
	readErr |= fread(prgBank,sizeof(prgBank),1,stateFile);
	readErr |= fread(cpuRam,sizeof(cpuRam),1,stateFile);
	readErr |= fread(&cpuA,sizeof(cpuA),1,stateFile);
	readErr |= fread(&cpuX,sizeof(cpuX),1,stateFile);
	readErr |= fread(&cpuY,sizeof(cpuY),1,stateFile);
	readErr |= fread(&cpuP,sizeof(cpuP),1,stateFile);
	readErr |= fread(&cpuS,sizeof(cpuS),1,stateFile);
	readErr |= fread(&cpuPC,sizeof(cpuPC),1,stateFile);
	readErr |= fread(chrBank,sizeof(chrBank),1,stateFile);
	readErr |= fread(chrSource,sizeof(chrSource),1,stateFile);
	readErr |= fread(oam,sizeof(oam),1,stateFile);
	readErr |= fread(nameSlot,sizeof(nameSlot),1,stateFile);
	readErr |= fread(ciram,sizeof(ciram),1,stateFile);
	readErr |= fread(palette,sizeof(palette),1,stateFile);
	readErr |= fread(&ppudot,sizeof(ppudot),1,stateFile);
	readErr |= fread(&vCounter,sizeof(vCounter),1,stateFile);
	readErr |= fread(&ppuW,sizeof(ppuW),1,stateFile);
	readErr |= fread(&ppuX,sizeof(ppuX),1,stateFile);
	readErr |= fread(&ppuT,sizeof(ppuT),1,stateFile);
	readErr |= fread(&ppuV,sizeof(ppuV),1,stateFile);
	readErr |= fread(&cart.mirroring,sizeof(cart.mirroring),1,stateFile);
	if (cart.wramSize)
		readErr |= fread(wram,cart.wramSize,1,stateFile);
	else if (cart.bwramSize)
		readErr |= fread(bwram,cart.bwramSize,1,stateFile);
	if (cart.cramSize)
		readErr |= fread(chrRam,cart.cramSize,1,stateFile);
	fclose(stateFile);
	prg_bank_switch();
	chr_bank_switch();
}
