#include "jemu.h"
#include <stdint.h>
#include "my_sdl.h"
#include "sms/smsemu.h"

uint8_t quit = 0, console = 0;
sdlSettings settings;
void select_console();

int main(){
	init_sdl(&settings);
	settings.renderQuality = "1";
	settings.window.name = "jEmu";
	settings.window.screenHeight = 600;
	settings.window.screenWidth = 800;
	settings.window.visible = 1;
	settings.window.winHeight = 600;
	settings.window.winWidth = 800;
	settings.window.winXPosition = 100;
	settings.window.winYPosition = 100;
	settings.window.xClip = 0;
	settings.window.yClip = 0;
	init_sdl_video();
	frameTime = 16666667;
	init_time(frameTime);
	if(!console){
		select_console();
	}
	if(smsemu()){
		printf("There was an error running smsemu\n");
		return 1;
	}
	close_sdl();
}

select_console(){
	while(!console){

	}
}
