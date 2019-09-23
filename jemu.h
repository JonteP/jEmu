#ifndef JEMU_H_
#define JEMU_H_

#include <stdint.h>
#include <linux/limits.h>
#include "my_sdl.h"

typedef enum _region {
	JAPAN,
	EXPORT
} Region;

typedef enum video {
	NTSC,
	PAL
} Video;

typedef enum console {
	NES,
	SMS
} Console;

struct machine {
	Console machine;
	char *bios;
	char cartFile[PATH_MAX];
	int  masterClock;
	Video videoSystem;
	Region region;
	int videoCard;
	int audioCard;
	uint8_t expansionSound;
};

extern uint8_t quit;
extern sdlSettings settings;
struct machine *currentMachine;

void (*reset_emulation)(void);

#endif /* JEMU_H_ */
