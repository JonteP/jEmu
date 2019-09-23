/*
 * smsemu.h
 *
 *  Created on: Jun 25, 2019
 *      Author: jonas
 */

#ifndef SMSEMU_H_
#define SMSEMU_H_
#include <stdint.h>
#include <linux/limits.h>
#include "../video/vdp.h"

// CLOCKS AND DIVIDERS
#define NTSC_MASTER			53693175
#define PAL_MASTER			53203424
#define CLOCK_DIV			15
#define Z80_CLOCK_DIV		CLOCK_DIV
#define FM_CLOCK_RATIO		72
#define FM_CLOCK_DIV		(CLOCK_DIV * FM_CLOCK_RATIO)
#define PSG_CLOCK_RATIO		16
#define PSG_CLOCK_DIV		(CLOCK_DIV * PSG_CLOCK_RATIO)
#define VDP_CLOCK_RATIO		3
#define VDP_CLOCK_DIV		(CLOCK_DIV / VDP_CLOCK_RATIO)


// INPUTS
#define IO1_PORTB_DOWN 		0x80
#define IO1_PORTB_UP 		0x40
#define IO1_PORTA_TR		0x20
#define IO1_PORTA_TL		0x10
#define IO1_PORTA_RIGHT		0x08
#define IO1_PORTA_LEFT		0x04
#define IO1_PORTA_DOWN		0x02
#define IO1_PORTA_UP		0x01
#define IO2_PORTB_TH		0x80
#define IO2_PORTA_TH		0x40
#define IO2_RESET			0x10
#define IO2_PORTB_TR		0x08
#define IO2_PORTB_TL		0x04
#define IO2_PORTB_RIGHT		0x02
#define IO2_PORTB_LEFT		0x01
#define IOCONTROL_PORTB_TH_LEVEL 		0x80
#define IOCONTROL_PORTB_TR_LEVEL 		0x40
#define IOCONTROL_PORTA_TH_LEVEL 		0x20
#define IOCONTROL_PORTA_TR_LEVEL 		0x10
#define IOCONTROL_PORTB_TH_DIRECTION 	0x08
#define IOCONTROL_PORTB_TR_DIRECTION 	0x04
#define IOCONTROL_PORTA_TH_DIRECTION 	0x02
#define IOCONTROL_PORTA_TR_DIRECTION 	0x01

extern uint8_t quit, ioPort1, ioPort2, ioControl, region, reset;
extern char cartFile[PATH_MAX], cardFile[PATH_MAX], expFile[PATH_MAX], biosFile[PATH_MAX];
extern FILE *logfile;
extern struct machine ntsc_us, ntsc_jp, pal1, pal2;
void set_timings(uint8_t), iocontrol_write(uint8_t), machine_menu_option(int);
int smsemu(void);

#endif /* SMSEMU_H_ */
