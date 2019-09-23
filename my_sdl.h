#ifndef MY_SDL_H_
#define MY_SDL_H_

#include <stdint.h>
#include "SDL.h"

#define MAX_MENU_ITEMS			25
#define MAX_MENU_ITEM_LENGTH	200

typedef void (*io_function)();
typedef struct windowHandle_ {
	SDL_Window *win;
	SDL_Renderer *rend;
	SDL_Texture *tex;
	char *name;
	int index;
	int winXPosition;
	int winYPosition;
	int winWidth;
	int winHeight;
	int screenWidth;
	int screenHeight;
	int windowID;
	int visible;
	int xClip;
	int yClip;
} windowHandle;
typedef struct sdlSettings {
	const char* renderQuality;
	uint8_t* ctable;
	int audioFrequency;
	int channels;
	int audioBufferSize;
	windowHandle window;
	int desktopWidth;
	int desktopHeight;
} sdlSettings;
typedef struct menuItem menuItem;

typedef enum _type {
	VERTICAL,
	HORIZONTAL,
	CENTERED
} Type;

struct menuItem {
	int length;
	char name[MAX_MENU_ITEMS][MAX_MENU_ITEM_LENGTH];
	Type type;
	int xOffset[MAX_MENU_ITEMS];
	int yOffset[MAX_MENU_ITEMS];
	int width;
	int height;
	int margin;
	menuItem *parent;
	io_function ioFunction;
};
extern uint_fast8_t isPaused, stateSave, stateLoad;
extern uint16_t channelMask, rhythmMask;
extern float frameTime, fps;
extern int clockRate;

void render_frame(uint32_t *), init_sdl(sdlSettings*), init_sdl_video(void), init_sdl_audio(void), close_sdl(void), init_sounds(void), output_sound(float *, int), destroy_handle (windowHandle *), init_time(float), toggle_menu(void);
void (*player1_button1)(uint8_t),
	 (*player1_button2)(uint8_t),
	 (*player1_buttonStart)(uint8_t),
	 (*player1_buttonSelect)(uint8_t),
	 (*player1_buttonUp)(uint8_t),
	 (*player1_buttonDown)(uint8_t),
	 (*player1_buttonLeft)(uint8_t),
	 (*player1_buttonRight)(uint8_t);
#endif /* MY_SDL_H_ */
