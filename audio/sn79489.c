/* HARDWARE NOTES
 * -in the Master System, Game Gear and Mega Drive, this is integrated in the VDP
 * -the Game Gear uses a modified version with stereo support, accessed via the 7 first z80 ports
 *
 * Version differences:
 * -in SC3000 (and possibly others) noise channel uses 0x0003 for feedback and 15bit shift reg
 * -the VDP integrated version uses 0x0009 and 16bit register instead
 *
 * TODO:
 * -some writes may reset other regs..
 * -possibly use lookup tables for speed
 * -output should be unsigned, with a highpass filter applied (makes difference for samples)
 * (see discussion: https://forums.nesdev.com/viewtopic.php?f=23&t=15562)
 * sn76489.c
 *
 */

#define FRAC_BITS			16

#include "sn79489.h"
#include <stdio.h>
#include <stdint.h>
#include "my_sdl.h"
#include "smsemu.h"
float *sn79489_SampleBuffer, sn79489_Sample = 0;
static uint32_t sampleRate, originalSampleRate;
const float volume_table[16]={ .32767, .26028, .20675, .16422, .13045, .10362, .08231, .06568, //could be made integer?
    .05193, .04125, .03277, .02603, .02067, .01642, .01304, 0 };
uint8_t noiseVolume, noiseRegister, currentReg, noisePhase, sn79489_mute;
float noiseOutput;
uint16_t noiseCounter, noiseShifter, noiseReload;
static int origSampleCounter = 0, bufferSize;
int psgAccumulatedCycles = 0, sn79489_SampleCounter = 0, audioCyclesToRun = 0;
struct ToneChannel tone0, tone1, tone2;
static inline int parity(int);
static inline void run_tone_channel(struct ToneChannel *);

void init_sn79489(int buffer){
	bufferSize = buffer;
	if(sn79489_SampleBuffer)
		free(sn79489_SampleBuffer);
	sn79489_SampleBuffer = (float *)malloc(bufferSize * sizeof(float));
	tone0.volume = tone1.volume = tone2.volume = noiseVolume = 0xf;
	tone0.reg = tone1.reg = tone2.reg = noiseReload = tone0.phase = tone1.phase = tone2.phase = 0;
	tone0.output = tone1.output = tone2.output = noiseOutput = 0;
	noiseShifter = 0x8000;
}

void set_timings_sn79489(int div, int clock){
	originalSampleRate = sampleRate = ((uint64_t)clock << FRAC_BITS) / div;
	origSampleCounter = 0;
}

void close_sn79489(){
	//move to smsemu
	free(sn79489_SampleBuffer);
}

void write_sn79489(uint8_t value){
	if(value & 0x80)
		currentReg = (value & 0x70);
	switch(currentReg){
	case 0x00: /* Tone 0 	*/
		if(value & 0x80)
			tone0.reg = ((tone0.reg & 0x3f0) | (value & 0xf));
		else
			tone0.reg = ((tone0.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x10: /* Volume 0 	*/
		tone0.volume = (value & 0xf);
		break;
	case 0x20: /* Tone 1 	*/
		if(value & 0x80)
			tone1.reg = ((tone1.reg & 0x3f0) | (value & 0xf));
		else
			tone1.reg = ((tone1.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x30: /* Volume 1 	*/
		tone1.volume = (value & 0xf);
		break;
	case 0x40: /* Tone 2 	*/
		if(value & 0x80)
			tone2.reg = ((tone2.reg & 0x3f0) | (value & 0xf));
		else
			tone2.reg = ((tone2.reg & 0xf) | ((value & 0x3f) << 4));
		break;
	case 0x50: /* Volume 2 	*/
		tone2.volume = (value & 0xf);
		break;
	case 0x60: /* Noise 	*/
		noiseRegister = (value & 0xf);
		noiseShifter = 0x8000;
		switch(noiseRegister & 0x03){
		case 0:
			noiseReload = 0x10;
			break;
		case 1:
			noiseReload = 0x20;
			break;
		case 2:
			noiseReload = 0x40;
			break;
		case 3:
			noiseReload = tone2.reg;
			break;
		}
		break;
	case 0x70: /* Volume 3 	*/
		noiseVolume = (value & 0xf);
		break;
	}
}

void run_tone_channel(struct ToneChannel *channel){
	if(channel->counter){
		channel->counter--;
		if(!channel->counter){
			channel->counter = channel->reg;
			channel->phase ^= 1;
		}
		channel->output = (channel->phase ? volume_table[channel->volume] : 0);
	} else{
		channel->counter = channel->reg;
	}
}

void run_sn79489(){
while(audioCyclesToRun){
	run_tone_channel(&tone0);
	run_tone_channel(&tone1);
	run_tone_channel(&tone2);
	if(noiseCounter){
		noiseCounter--;
		if(!noiseCounter){
			noisePhase ^= 1;
			noiseCounter = noiseReload;
			if(noisePhase){
				noiseOutput = ((noiseShifter & 0x01) ? volume_table[noiseVolume] : 0);
				noiseShifter = ((noiseShifter >> 1) | (((noiseRegister & 0x04) ? parity(noiseShifter & 0x09) : (noiseShifter & 0x01)) << 15));
			}
		}
	}
	else
		noiseCounter = noiseReload;
	if(!sn79489_mute)
		sn79489_Sample += ((tone0.output + tone1.output + tone2.output + noiseOutput) / (4*volume_table[0])); /* TODO: less hackish */
	else
		sn79489_Sample += 0;

	//move this to smsemu?
	origSampleCounter++;
	if(origSampleCounter == (sampleRate >> FRAC_BITS)){
		sn79489_SampleBuffer[sn79489_SampleCounter++] = (float)(sn79489_Sample / origSampleCounter);
		if(sn79489_SampleCounter == bufferSize){
		//	output_sound(sn79489_SampleBuffer, sn79489_SampleCounter);
			sn79489_SampleCounter = 0;
		}
		sampleRate = originalSampleRate + sampleRate - (origSampleCounter << FRAC_BITS);
		sn79489_Sample = 0;
		origSampleCounter = 0;
	}
	audioCyclesToRun--;
}
}

int parity(int val){
    val^=val>>8;
    val^=val>>4;
    val^=val>>2;
    val^=val>>1;
    return val&1;
};
