/* TODO:
 * -scaleable menus
 */

/* Good source for text boxes: https://gamedev.stackexchange.com/questions/140294/what-is-the-most-efficient-way-to-render-a-textbox-in-c-sdl2
 * Good source for file browsing: https://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
 */
#include "my_sdl.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <bsd/string.h>
#include <time.h> 	/* clock */
#include <unistd.h> /* usleep */
#include <dirent.h>
#include <sys/stat.h>
#include "sms/smsemu.h"
#include "audio/sn79489.h"
#include "audio/ym2413.h"
#include "cpu/z80.h"
#include "sms/smscartridge.h"
#include "jemu.h"
#include "nes/fds.h"

#define RENDER_FLAGS	(SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)

SDL_AudioSpec wantedAudioSettings, audioSettings;
SDL_Event event;
SDL_DisplayMode current;
SDL_Texture *whiteboard = NULL;
TTF_Font* Sans;
SDL_Color menuTextColor = {0xff, 0xff, 0xff, 0x00};
uint8_t menuBgColor[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t menuActiveColor[4] = {0x80, 0x80, 0x80, 0x00};
uint_fast8_t isPaused = 0, fullscreen = 0, stateSave = 0, stateLoad = 0, vsync = 0, throttle = 1, showMenu = 0;
sdlSettings *currentSettings;
menuItem prototypeMenu, mainMenu, fileMenu, graphicsMenu, machineMenu, audioMenu, fileList, machineList, *currentMenu;
io_function io_func;
uint8_t currentMenuColumn = 0, currentMenuRow = 0, oldMenuRow = 0, menuFontSize = 18, filesLeft = 0;
uint16_t channelMask = 0x1ff, rhythmMask = 0x1f;
DIR *currentDir;
char *defaultDir = "/home/jonas/git/roms/", workDir[PATH_MAX];
int fileListOffset = 0, oldFileListOffset = 0;
float frameTime, fps;
int clockRate;
SDL_DisplayMode mode;
SDL_Rect SrcR, TrgR;

static inline void render_window (windowHandle *, uint32_t *), idle_time(float), create_handle (windowHandle *), draw_menu(menuItem *), set_menu(void), get_menu_size(menuItem *, int, int), get_max_menu_size(menuItem *), create_menu(void), main_menu_option(int), clear_screen(SDL_Renderer *);
static inline void option_fullscreen(void), option_quit(void), option_open_file(void), game_io(void), menu_io(void), file_io(void), get_parent_dir(char *), add_slash(char *), set_screen_cropratio(windowHandle *handle);
static inline float diff_time(struct timespec *, struct timespec *);
static inline int is_directory(const char *), create_file_list(void), file_count(DIR *), fileSorter(const void *const, const void *const);
static inline struct dirent ** read_directory(DIR *);
void (*current_options)();

/*****************/
/* SDL FUNCTIONS */
/*****************/

void init_sdl(sdlSettings *settings) {
    io_func = &game_io;
    current_options = &main_menu_option;
    currentSettings = settings;
    SDL_version ver;
    SDL_GetVersion(&ver);
    printf("Running SDL version: %d.%d.%d\n",ver.major,ver.minor,ver.patch);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    if(TTF_Init()==-1) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);}
}

void init_sdl_video(){
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,currentSettings->renderQuality);
	destroy_handle(&currentSettings->window);
	create_handle (&currentSettings->window);
	currentSettings->window.index = SDL_GetWindowDisplayIndex(currentSettings->window.win);
	SDL_GetDesktopDisplayMode(currentSettings->window.index, &mode);
	currentSettings->desktopWidth = mode.w;
	currentSettings->desktopHeight = mode.h;
	SDL_DestroyTexture(whiteboard);
	if((whiteboard = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, currentSettings->window.winWidth, currentSettings->window.winHeight)) == NULL){
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	if(Sans)
		TTF_CloseFont(Sans);
	menuFontSize = (currentSettings->window.winHeight >> 5);
	Sans = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", menuFontSize);
	if(!Sans){
		printf("TTF_OpenFont failed: %s\n", TTF_GetError());
		exit(EXIT_FAILURE);}
	set_screen_cropratio(&currentSettings->window);
	create_menu();
}

void init_sdl_audio(){
	wantedAudioSettings.freq = currentSettings->audioFrequency;
	wantedAudioSettings.format = AUDIO_F32;
	wantedAudioSettings.channels = currentSettings->channels;
	wantedAudioSettings.callback = NULL;
	wantedAudioSettings.samples = currentSettings->audioBufferSize >> 1; /* Must be less than buffer size to prevent increasing lag... */
	SDL_CloseAudio();
	if (SDL_OpenAudio(&wantedAudioSettings, &audioSettings) < 0)
	    SDL_Log("Failed to open audio: %s", SDL_GetError());
	else if (audioSettings.format != wantedAudioSettings.format)
	    SDL_Log("The desired audio format is not available.");
	SDL_PauseAudio(0);
	SDL_ClearQueuedAudio(1);
}

void create_handle (windowHandle *handle){
	/* TODO: add clipping options here */
	if((handle->win = SDL_CreateWindow(handle->name, handle->winXPosition, handle->winYPosition, handle->winWidth, handle->winHeight, SDL_WINDOW_RESIZABLE)) == NULL){
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	if((handle->rend = SDL_CreateRenderer(handle->win, -1, RENDER_FLAGS)) == NULL){
		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	if((handle->tex = SDL_CreateTexture(handle->rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, handle->screenWidth, handle->screenHeight)) == NULL){
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	handle->windowID = SDL_GetWindowID(handle->win);
}

void destroy_handle (windowHandle *handle){
	if(handle->tex)
		SDL_DestroyTexture(handle->tex);
	if(handle->rend)
		SDL_DestroyRenderer(handle->rend);
	if(handle->win) {
	    SDL_GetWindowPosition(handle->win, &handle->winXPosition, &handle->winYPosition);
		SDL_DestroyWindow(handle->win);
	}
}

void close_sdl(){
	destroy_handle (&currentSettings->window);
	TTF_CloseFont(Sans);
	SDL_ClearQueuedAudio(1);
	SDL_CloseAudio();
	SDL_Quit();
}

void set_screen_cropratio(windowHandle *handle){
	SrcR.x = handle->xClip;
	SrcR.y = handle->yClip;
	SrcR.w = handle->screenWidth - (handle->xClip << 1);
	SrcR.h = handle->screenHeight - (handle->yClip << 1);
	TrgR.x = (currentSettings->desktopWidth >> 3);
	TrgR.y = 0;
	TrgR.w = currentSettings->desktopWidth - (currentSettings->desktopWidth >> 2);
	TrgR.h = currentSettings->desktopHeight;
}

void render_window (windowHandle * handle, uint32_t * buffer){
	void *pix;
	int pitch;
	if(SDL_LockTexture(handle->tex, NULL, &pix, &pitch)){
		printf("SDL_LockTexture failed: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);}
	memcpy(pix, buffer, pitch * handle->screenHeight);
	SDL_UnlockTexture(handle->tex);
	SDL_SetRenderTarget(handle->rend, whiteboard);
	SDL_RenderCopy(handle->rend, handle->tex, &SrcR, NULL);
	if(showMenu){
		draw_menu(currentMenu);
		if(currentMenu->parent)
			draw_menu(currentMenu->parent);
	}
	SDL_SetRenderTarget(handle->rend, NULL);
	if(fullscreen)
		SDL_RenderCopy(handle->rend, whiteboard, NULL, &TrgR);
	else
		SDL_RenderCopy(handle->rend, whiteboard, NULL, NULL);
	SDL_RenderPresent(handle->rend);
}

void clear_screen(SDL_Renderer *rend){
	SDL_SetRenderDrawColor(rend, 0, 0, 0, 0xff);
	SDL_RenderClear(rend);
	SDL_RenderPresent(rend);
}

/****************/
/* TIME KEEPING */
/****************/

struct timespec throttleClock, startClock, endClock;
int frameCounter;
void init_time (float time){
	frameCounter = 0;
	clock_gettime(CLOCK_MONOTONIC, &startClock);
	clock_getres(CLOCK_MONOTONIC, &throttleClock);
	clock_gettime(CLOCK_MONOTONIC, &throttleClock);
	throttleClock.tv_nsec += time;
	throttleClock.tv_sec += throttleClock.tv_nsec / 1000000000;
	throttleClock.tv_nsec %= 1000000000;
}
float xfps;
void idle_time (float time){
	frameCounter++;
	if(frameCounter == 60){
		clock_gettime(CLOCK_MONOTONIC, &endClock);
		xfps = (float)((frameCounter / diff_time(&startClock, &endClock) + xfps) * .5);
		if(!throttle){
			fps = xfps;
			printf("%f\n",fps);
			set_timings(2);
		}
		frameCounter = 0;
		clock_gettime(CLOCK_MONOTONIC, &startClock);
	}
	if(throttle){
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &throttleClock, NULL);
	throttleClock.tv_nsec += time;
	throttleClock.tv_sec += throttleClock.tv_nsec / 1000000000;
	throttleClock.tv_nsec %= 1000000000;
	}
}
float diff_time(struct timespec *start, struct timespec *end){
	float temp;
	if ((end->tv_nsec-start->tv_nsec)<0) {
		temp = end->tv_sec - start->tv_sec - 1;
		temp += (1 + ((float)(end->tv_nsec-start->tv_nsec) / 1000000000));
	} else {
		temp = end->tv_sec - start->tv_sec;
		temp += ((float)(end->tv_nsec-start->tv_nsec) / 1000000000);
	}
	return temp;
}

void render_frame(uint32_t *buffer){
	render_window (&currentSettings->window, buffer);
	idle_time(frameTime);
	io_func();
}

/****************/
/* FILE BROWSER */
/****************/

int is_directory(const char *str){
    struct stat path_stat;
    if(!stat(str, &path_stat)){
        return S_ISDIR(path_stat.st_mode);
    }
    else
    	return -1;
}

int file_count(DIR *dir){
	int fileCounter = 0;
	rewinddir(dir);
	while (readdir(dir))
		fileCounter++;
	return fileCounter;
}

void add_slash(char *dir){
	if(strcmp(dir + strlen(dir) - 1, "/"))
		sprintf(dir,"%s/",dir);
}

void get_parent_dir(char *str){
	add_slash(str); /* set string to known state */
	str[strlen(str) - 1] = '\0';
	char *ptr = strrchr(str, '/');
	if(ptr)
		*ptr = '\0';
	add_slash(str);
	/* Retrieve position of parent dir. TODO: more sophisticated */
	fileListOffset = oldFileListOffset;
	currentMenuRow = oldMenuRow;
	oldFileListOffset = 0;
	oldMenuRow = 1;
	create_file_list();
}

void append_to_dir(char *dir, const char *str){
	if(!strcmp(str, ".."))
		get_parent_dir(dir);
	else if(strcmp(str, ".")){
		sprintf(dir,"%s%s",dir,str);
		if(is_directory(dir) == 1){
			add_slash(dir);
			oldFileListOffset = fileListOffset;
			oldMenuRow = currentMenuRow;
			fileListOffset = 0;
			currentMenuRow = 1;
			create_file_list();
		}
		else if(!is_directory(dir)){
			toggle_menu();
			clear_screen(currentSettings->window.rend);
			strcpy(currentMachine->cartFile,dir);
			get_parent_dir(dir);
			reset_emulation();
		}
		else
			printf("Error: no such file or directory.\n");
	}
}

int fileSorter(const void *const file1, const void *const file2){
    return strcmp((*(struct dirent **) file1)->d_name, (*(struct dirent **) file2)->d_name);
}

struct dirent ** read_directory(DIR *dir){
	struct dirent *entry, **files;
	int counter = 0;
	int length = file_count(dir);
	files = malloc(length * sizeof(*files));
	rewinddir(dir);
	while((entry = readdir(dir)) != NULL)
		files[counter++] = entry;
	qsort(files, length, sizeof(*files), fileSorter);
	return files;
}

int create_file_list(){
	struct dirent **sortedFiles;
	uint8_t counter = 0;
	if(!(currentDir = opendir(workDir))){
		printf("Error: failed to open directory: %s\n",workDir);
		return 1;
	}
	int length = file_count(currentDir);
	sortedFiles = read_directory(currentDir);
	filesLeft = 1;
	for(int i = 0; i < MAX_MENU_ITEMS; i++){
		if((i + fileListOffset) < length){
			strcpy(fileList.name[i], sortedFiles[i + fileListOffset]->d_name);
			counter++;
		}
		else
			filesLeft = 0;
	}
	closedir(currentDir);
	free(sortedFiles);
	fileList.length = counter;
	get_menu_size(&fileList, 0, 0);
	return 0;
}

/*********/
/* MENUS */
/*********/

void create_menu(){	/* define menus */
	prototypeMenu.margin = 4;
	prototypeMenu.ioFunction = &menu_io;
	prototypeMenu.parent = NULL;
	prototypeMenu.width = -1;
	prototypeMenu.height = -1;
	mainMenu = fileMenu = machineMenu = graphicsMenu = audioMenu = fileList = machineList = prototypeMenu;

	mainMenu.length = 4;
	strcpy(mainMenu.name[0], "File");
	strcpy(mainMenu.name[1], "Machine");
	strcpy(mainMenu.name[2], "Graphics");
	strcpy(mainMenu.name[3], "Audio");
	mainMenu.type = HORIZONTAL;
	get_menu_size(&mainMenu, 0, 0);

	fileMenu.length = 4;
	strcpy(fileMenu.name[0], "Load ROM...");
	strcpy(fileMenu.name[1], "Load State");
	strcpy(fileMenu.name[2], "Save State");
	strcpy(fileMenu.name[3], "Quit");
	fileMenu.type = VERTICAL;
	fileMenu.parent = &mainMenu;
	get_menu_size(&fileMenu, mainMenu.xOffset[0] - mainMenu.margin, mainMenu.height + (mainMenu.margin << 1));

	machineMenu.length = 2;
	strcpy(machineMenu.name[0], "Emulated Machine...");
	strcpy(machineMenu.name[1], "Throttle (F10)");
	machineMenu.type = VERTICAL;
	machineMenu.parent = &mainMenu;
	get_menu_size(&machineMenu, mainMenu.xOffset[1] - mainMenu.margin, mainMenu.height + (mainMenu.margin << 1));

	graphicsMenu.length = 4;
	strcpy(graphicsMenu.name[0], "Fullscreen (F11)");
	strcpy(graphicsMenu.name[1], "Toggle Sprites");
	strcpy(graphicsMenu.name[2], "Toggle Background");
	strcpy(graphicsMenu.name[3], "Settings...");
	graphicsMenu.type = VERTICAL;
	graphicsMenu.parent = &mainMenu;
	get_menu_size(&graphicsMenu, mainMenu.xOffset[2] - mainMenu.margin, mainMenu.height + (mainMenu.margin << 1));

	audioMenu.length = 3;
	strcpy(audioMenu.name[0], "Set Samplerate");
	strcpy(audioMenu.name[1], "Toggle Channels...");
	strcpy(audioMenu.name[2], "Mute");
	audioMenu.type = VERTICAL;
	audioMenu.parent = &mainMenu;
	get_menu_size(&audioMenu, mainMenu.xOffset[3] - mainMenu.margin, mainMenu.height + (mainMenu.margin << 1));

	machineList.length = 7;
	machineList.margin = 0;
	strcpy(machineList.name[0], "Sega Master System - NTSC (Japan)");
	strcpy(machineList.name[1], "Sega Master System - NTSC (US)");
	strcpy(machineList.name[2], "Sega Master System - PAL [VDP 1]");
	strcpy(machineList.name[3], "Sega Master System - PAL [VDP 2]");
	strcpy(machineList.name[4], "Nintendo Entertainment System - NTSC (Japan)");
	strcpy(machineList.name[5], "Nintendo Entertainment System - NTSC (US)");
	strcpy(machineList.name[6], "Nintendo Entertainment System - PAL");
	machineList.type = CENTERED;
	get_menu_size(&machineList, 0, 0);

	fileList.ioFunction = &file_io;
	fileList.type = CENTERED;
	fileList.width = (currentSettings->window.winWidth >> 1);
	fileList.margin = 0;

	if(!defaultDir)
		getcwd(workDir, sizeof(workDir));
	else
		strcpy(workDir, defaultDir);
	add_slash(workDir);
	currentMenu = &mainMenu;
}

void get_max_menu_size(menuItem *menu){
	int width, height, maxWidth = 0, maxHeight = 0;
	for(int i = 0; i < menu->length; i++){
		TTF_SizeText(Sans, menu->name[i], &width, &height);
		if(width > maxWidth)
			maxWidth = width;
		if(height > maxHeight)
			maxHeight = height;
	}
	if(menu->height < 0)
		menu->height = maxHeight;
	if(menu->width < 0)
		menu->width = maxWidth;
}

void get_menu_size(menuItem *menu, int xoff, int yoff){
	if(menu->height < 0 || menu->width < 0)
		get_max_menu_size(menu);
	int width, height;
		for(int i = 0; i < menu->length; i++){
			TTF_SizeText(Sans, menu->name[i], &width, &height);
			menu->xOffset[i] = xoff + menu->margin;
			menu->yOffset[i] = yoff + menu->margin;
			if(menu->type == HORIZONTAL)
				xoff += (width + (menu->margin << 1));
			else
				yoff += (height + (menu->margin << 1));
		if(menu->type == CENTERED){
			menu->xOffset[i] += ((currentSettings->window.winWidth >> 1) - (menu->width >> 1));
			menu->yOffset[i] += ((currentSettings->window.winHeight >> 1) - (menu->height * (menu->length >> 1)));
		}
	}
}

void set_menu(){
	switch(currentMenuColumn){
	case 0:
		currentMenu = &fileMenu;
		break;
	case 1:
		currentMenu = &machineMenu;
		break;
	case 2:
		currentMenu = &graphicsMenu;
		break;
	case 3:
		currentMenu = &audioMenu;
		break;
	}
	if(currentMenuRow > currentMenu->length)
		currentMenuRow = currentMenu->length;
}

void toggle_menu(){
	showMenu ^= 1;
	if(showMenu){
		isPaused = 1;
		io_func = mainMenu.ioFunction;
	}
	else if(!showMenu){
		isPaused = 0;
		io_func = game_io;
	}
	currentMenu = &mainMenu;
	currentMenuColumn = currentMenuRow = fileListOffset = 0;
	current_options = &main_menu_option;
}

void draw_menu(menuItem *menu){
	SDL_Rect menuRect, srcRect, destRect;
	SDL_Texture *text;
	SDL_Surface *menuText;
	for(int i = 0; i < menu->length; i++){
	    menuText = TTF_RenderText_Blended(Sans, menu->name[i], menuTextColor);
	    text = SDL_CreateTextureFromSurface(currentSettings->window.rend, menuText);
	    SDL_FreeSurface(menuText);
	    SDL_QueryTexture(text, NULL, NULL, &menuRect.w, &menuRect.h);
	    if(menu->width < menuRect.w)
	    	srcRect.w = menu->width;
	    else
	    	srcRect.w = menuRect.w;
	    srcRect.h = menuRect.h;
		srcRect.x = 0;
		srcRect.y = 0;

		destRect = srcRect;
	    destRect.x = menu->xOffset[i];
	    destRect.y = menu->yOffset[i];

	    if(menu->type != HORIZONTAL)
			menuRect.w = menu->width;
	    menuRect.w += (menu->margin << 1);
	    menuRect.h += (menu->margin << 1);
	    menuRect.x = (menu->xOffset[i] - menu->margin);
		menuRect.y = (menu->yOffset[i] - menu->margin);

		/* Highlight active menu item */
	    if(menu->type == HORIZONTAL && currentMenuColumn == i && !currentMenuRow)
	        SDL_SetRenderDrawColor(currentSettings->window.rend, menuActiveColor[0], menuActiveColor[1], menuActiveColor[2], menuActiveColor[3]);
	    else if(menu->type != HORIZONTAL && currentMenuRow == i + 1)
	        SDL_SetRenderDrawColor(currentSettings->window.rend, menuActiveColor[0], menuActiveColor[1], menuActiveColor[2], menuActiveColor[3]);
	    else
	    	SDL_SetRenderDrawColor(currentSettings->window.rend, menuBgColor[0], menuBgColor[1], menuBgColor[2], menuBgColor[3]);
	    SDL_RenderFillRect(currentSettings->window.rend, &menuRect);
	    SDL_RenderCopy(currentSettings->window.rend, text, &srcRect, &destRect);
	    SDL_DestroyTexture(text);
	}
}

void main_menu_option(int option){
	switch(option){
	case 0x01:
		io_func = &file_io;
		option_open_file();
		break;
	case 0x04:
		option_quit();
		break;
	case 0x11:
		currentMenu = &machineList;
		current_options = &machine_menu_option;
		break;
	case 0x21:
		option_fullscreen();
		break;
	}
}

void output_sound(float *buffer, int counter){
//	float sample[counter];
	if (!throttle || (SDL_GetQueuedAudioSize(1) > (audioSettings.size * audioSettings.channels)))
		SDL_ClearQueuedAudio(1);
//	for(int i = 0;i < counter;i++){
//		sample[i] = (ym2413_SampleBuffer[i] + sn79489_SampleBuffer[i]) / 2;
//	}
	//sn79489_SampleCounter = 0;
	if (SDL_QueueAudio(1, buffer, counter * sizeof(*buffer)))
		printf("SDL_QueueAudio failed: %s\n", SDL_GetError());
}

/****************/
/* MENU OPTIONS */
/****************/

void option_open_file(){
	create_file_list();
	currentMenu = &fileList;
}

void option_fullscreen(){
	fullscreen ^= 1;
	if (fullscreen){
		SDL_SetWindowDisplayMode(currentSettings->window.win, &mode);
		SDL_SetWindowFullscreen(currentSettings->window.win, SDL_WINDOW_FULLSCREEN);
	    SDL_ShowCursor(SDL_DISABLE);
		SDL_SetWindowGrab(currentSettings->window.win, SDL_TRUE);
		clear_screen(currentSettings->window.rend);
	}
	else if (!fullscreen){
		SDL_SetWindowFullscreen(currentSettings->window.win, 0);
	    SDL_ShowCursor(SDL_ENABLE);
		SDL_SetWindowGrab(currentSettings->window.win, SDL_FALSE);
	}
}

void option_quit(){
	printf("Quitting\n");
	quit = 1;
	isPaused = 0;
}

/******************/
/* INPUT HANDLERS */
/******************/

void menu_io(){
	while (SDL_PollEvent(&event)){
		if (event.window.windowID == currentSettings->window.windowID){
			if(event.type == SDL_KEYDOWN){
				switch (event.key.keysym.scancode){
				case SDL_SCANCODE_UP:
					if(currentMenuRow){
						currentMenuRow--;
						if(!currentMenuRow  && currentMenu->parent != NULL)
							currentMenu = &mainMenu;
						else
							currentMenuRow = 1;
					}
					break;
				case SDL_SCANCODE_DOWN:
					if(!currentMenuRow)
						set_menu();
					if(currentMenuRow < (currentMenu->length))
						currentMenuRow++;
					break;
				case SDL_SCANCODE_LEFT:
					if(currentMenuColumn){
						currentMenuColumn--;
						set_menu();
					}
					break;
				case SDL_SCANCODE_RIGHT:
					if((currentMenuColumn < (mainMenu.length) - 1)){
						currentMenuColumn++;
						set_menu();
					}
					break;
				case SDL_SCANCODE_RETURN:
					current_options((currentMenuColumn << 4) | currentMenuRow);
					break;
				case SDL_SCANCODE_Q:
					if (!(event.key.repeat))
						toggle_menu();
					break;
				default:
					break;
				}
			}
		}
	}
}

void file_io(){
	while (SDL_PollEvent(&event)){
		if (event.window.windowID == currentSettings->window.windowID){
		if(event.type == SDL_KEYDOWN){
			switch (event.key.keysym.scancode){
			case SDL_SCANCODE_UP:
				currentMenuRow--;
				if(currentMenuRow <= 0){
					if(fileListOffset > 0){
						fileListOffset--;
						create_file_list();
					}
					currentMenuRow = 1;
				}
				break;
			case SDL_SCANCODE_DOWN:
				if(currentMenuRow <= currentMenu->length)
					currentMenuRow++;
				if(currentMenuRow > currentMenu->length){
					currentMenuRow = currentMenu->length;
					if(filesLeft){
						fileListOffset++;
						create_file_list();
					}
				}
				break;
			case SDL_SCANCODE_RETURN:
				append_to_dir(workDir, fileList.name[currentMenuRow - 1]);
				break;
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_BACKSPACE:
				get_parent_dir(workDir);
				break;
			case SDL_SCANCODE_Q:
				if (!(event.key.repeat))
					toggle_menu();
				break;
			default:
				break;
			}
		}
		}
	}
}

void game_io(){
	while (SDL_PollEvent(&event)){
		if (event.window.windowID == currentSettings->window.windowID){
		switch (event.type){
		case SDL_KEYDOWN:
			switch (event.key.keysym.scancode){
			case SDL_SCANCODE_SPACE:
			    if (!(event.key.repeat)) {
			        currentDiskSide ^= 1;
			    }
			    break;
            case SDL_SCANCODE_LALT:
                if (!(event.key.repeat)) {
                    diskFlag ^= 1;
                }
                break;
			case SDL_SCANCODE_1:
				channelMask ^= 1;
				break;
			case SDL_SCANCODE_2:
				channelMask ^= (1 << 1);
				break;
			case SDL_SCANCODE_3:
				channelMask ^= (1 << 2);
				break;
			case SDL_SCANCODE_4:
				channelMask ^= (1 << 3);
				break;
			case SDL_SCANCODE_5:
				channelMask ^= (1 << 4);
				break;
			case SDL_SCANCODE_6:
				channelMask ^= (1 << 5);
				break;
			case SDL_SCANCODE_7:
				channelMask ^= (1 << 6);
				break;
			case SDL_SCANCODE_8:
				channelMask ^= (1 << 7);
				break;
			case SDL_SCANCODE_9:
				rhythmMask ^= (1 << 8);
				break;
			case SDL_SCANCODE_W:
				rhythmMask ^= (1 << 0);
				break;
			case SDL_SCANCODE_E:
				rhythmMask ^= (1 << 1);
				break;
			case SDL_SCANCODE_R:
				rhythmMask ^= (1 << 2);
				break;
			case SDL_SCANCODE_T:
				rhythmMask ^= (1 << 3);
				break;
			case SDL_SCANCODE_Y:
				rhythmMask ^= (1 << 4);
				break;
			case SDL_SCANCODE_ESCAPE:
				option_quit();
				break;
			case SDL_SCANCODE_F1:
				printf("Saving state\n");
				stateSave = 1;
				break;
			case SDL_SCANCODE_F2:
				printf("Loading state\n");
				stateLoad = 1;
				break;
			case SDL_SCANCODE_F3:
				printf("Resetting\n");
				reset = 1;
				isPaused = 0;
				break;
			case SDL_SCANCODE_F10:
				throttle ^= 1;
				if (throttle)
					init_time(frameTime);
				break;
			case SDL_SCANCODE_F11:
				option_fullscreen();
				break;
			case SDL_SCANCODE_F12:
				vsync ^= 1;
				if (vsync){
					SDL_DestroyRenderer(currentSettings->window.rend);
					currentSettings->window.rend = SDL_CreateRenderer(currentSettings->window.win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE|SDL_RENDERER_PRESENTVSYNC);
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, currentSettings->window.screenWidth, currentSettings->window.screenHeight);
					fps = mode.refresh_rate;
					set_timings(2);
					SDL_SetWindowDisplayMode(currentSettings->window.win, &mode);
					clear_screen(currentSettings->window.rend);
				}
				else{
					SDL_DestroyRenderer(currentSettings->window.rend);
					currentSettings->window.rend = SDL_CreateRenderer(currentSettings->window.win, -1, SDL_RENDERER_ACCELERATED);
					currentSettings->window.tex = SDL_CreateTexture(currentSettings->window.rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, currentSettings->window.screenWidth, currentSettings->window.screenHeight);
					set_timings(1);
				}
				break;
			case SDL_SCANCODE_P:
				if (!(event.key.repeat))
					isPaused ^= 1;
				break;
			case SDL_SCANCODE_UP:
				player1_buttonUp(1);
				//ioPort1 &= ~IO1_PORTA_UP;
				break;
			case SDL_SCANCODE_DOWN:
				player1_buttonDown(1);
				//ioPort1 &= ~IO1_PORTA_DOWN;
				break;
			case SDL_SCANCODE_LEFT:
				player1_buttonLeft(1);
				//ioPort1 &= ~IO1_PORTA_LEFT;
				break;
			case SDL_SCANCODE_RIGHT:
				player1_buttonRight(1);
				//ioPort1 &= ~IO1_PORTA_RIGHT;
				break;
			case SDL_SCANCODE_RETURN:
				player1_buttonStart(1);
				//z80_nmiPulled = 1;
				break;
			case SDL_SCANCODE_BACKSPACE:
				player1_buttonSelect(1);
				//ioPort2 &= ~IO2_RESET;
				break;
			case SDL_SCANCODE_Z:
				player1_button1(1);
			//	ioPort1 &= ~IO1_PORTA_TL;
				break;
			case SDL_SCANCODE_X:
				player1_button2(1);
				//ioPort1 &= ~IO1_PORTA_TR;
				break;
			case SDL_SCANCODE_I:
				ioPort1 &= ~IO1_PORTB_UP;
				break;
			case SDL_SCANCODE_K:
				ioPort1 &= ~IO1_PORTB_DOWN;
				break;
			case SDL_SCANCODE_J:
				ioPort2 &= ~IO2_PORTB_LEFT;
				break;
			case SDL_SCANCODE_L:
				ioPort2 &= ~IO2_PORTB_RIGHT;
				break;
			case SDL_SCANCODE_A:
				ioPort2 &= ~IO2_PORTB_TL;
				break;
			case SDL_SCANCODE_S:
				ioPort2 &= ~IO2_PORTB_TR;
				break;
			case SDL_SCANCODE_Q:
				if (!(event.key.repeat))
					toggle_menu();
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:
			switch (event.key.keysym.scancode){
			case SDL_SCANCODE_UP:
				player1_buttonUp(0);
				//ioPort1 |= IO1_PORTA_UP;
				break;
			case SDL_SCANCODE_DOWN:
				player1_buttonDown(0);
				//ioPort1 |= IO1_PORTA_DOWN;
				break;
			case SDL_SCANCODE_LEFT:
				player1_buttonLeft(0);
				//ioPort1 |= IO1_PORTA_LEFT;
				break;
			case SDL_SCANCODE_RIGHT:
				player1_buttonRight(0);
				//ioPort1 |= IO1_PORTA_RIGHT;
				break;
			case SDL_SCANCODE_BACKSPACE:
				player1_buttonSelect(0);
				//ioPort2 |= IO2_RESET;
				break;
			case SDL_SCANCODE_Z:
				player1_button1(0);
				//ioPort1 |= IO1_PORTA_TL;
				break;
			case SDL_SCANCODE_X:
				player1_button2(0);
				//ioPort1 |= IO1_PORTA_TR;
				break;
			case SDL_SCANCODE_I:
				ioPort1 |= IO1_PORTB_UP;
				break;
			case SDL_SCANCODE_K:
				ioPort1 |= IO1_PORTB_DOWN;
				break;
			case SDL_SCANCODE_J:
				ioPort2 |= IO2_PORTB_LEFT;
				break;
			case SDL_SCANCODE_L:
				ioPort2 |= IO2_PORTB_RIGHT;
				break;
			case SDL_SCANCODE_A:
				ioPort2 |= IO2_PORTB_TL;
				break;
			case SDL_SCANCODE_S:
				ioPort2 |= IO2_PORTB_TR;
				break;
			case SDL_SCANCODE_RETURN:
				player1_buttonStart(0);
				//z80_nmiPulled = 1;
				break;
			default:
				break;

			}
			break;
		}
		}
	}
}
