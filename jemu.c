/* TODO:
 * -FIR low pass filtering of sound
 * -save states
 */

#include "jemu.h"
#include <stdint.h>
#include "my_sdl.h"
#include "sms/smsemu.h"
#include "nes/nesemu.h"

uint8_t quit = 0, console = 0;
sdlSettings settings;
struct machine *currentMachine;
int run_console();

int main() {
	init_sdl(&settings);
	settings.renderQuality = "0";
	settings.window.name = "jEmu";
    settings.window.screenWidth = 800;
	settings.window.screenHeight = 600;
	settings.window.visible = 1;
    settings.window.winWidth = 320;
	settings.window.winHeight = 240;
	settings.window.winXPosition = 100;
	settings.window.winYPosition = 100;
	settings.window.xClip = 0;
	settings.window.yClip = 0;
	init_sdl_video();
	frameTime = 16666667;
	init_time(frameTime);
	currentMachine = &nes_ntsc;
	run_console();
	close_sdl();
}

void machine_menu_option(int option) {
	switch(option & 0x0f) {
	case 1:
		currentMachine = &ntsc_jp;
		break;
	case 2:
		currentMachine = &ntsc_us;
		break;
	case 3:
		currentMachine = &pal1;
		break;
	case 4:
		currentMachine = &pal2;
		break;
	case 5:
		currentMachine = &famicom;
		break;
	case 6:
		currentMachine = &nes_ntsc;
		break;
	case 7:
		currentMachine = &nes_pal;
		break;
	}
	toggle_menu();
	run_console();
}

int run_console() {
	if(currentMachine->machine == NES) {
		if(nesemu()) {
			printf("There was an error running nesemu\n");
			return 1;
		}
	}
	else if(currentMachine->machine == SMS) {
		if(smsemu()) {
			printf("There was an error running smsemu\n");
			return 1;
		}
	}
	return 0;
}
