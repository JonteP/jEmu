/* YM2413 aka OPLL
 * TODO: emulate the different derivatives of the chip
 * -snare drum sounds bad...
 */

#include "ym2413.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "my_sdl.h"
#include"smsemu.h"

#define FRAC_BITS				16
#define INSTRUMENT_CHANNELS		6
#define RHYTHM_CHANNELS			3
#define TOTAL_CHANNELS			(INSTRUMENT_CHANNELS + RHYTHM_CHANNELS)
#define RATE_MAX				63
#define ATTENUATION_MAX			127
#define ATTENUATION_MIN			0
#define	ATTENUATION_SILENT		124

static uint8_t instruments[0x10][0x8] =	{ /* Inaccurate, based on MAME code */
	{0x49, 0x4c, 0x4c, 0x12, 0x00, 0x00, 0x00, 0x00 },  /* 0. Custom 			*/
	{0x61, 0x61, 0x1e, 0x17, 0xf0, 0x78, 0x00, 0x17 },  /* 1. Violin 			*/
	{0x13, 0x41, 0x1e, 0x0d, 0xd7, 0xf7, 0x13, 0x13 },  /* 2. Guitar 			*/
	{0x13, 0x01, 0x99, 0x04, 0xf2, 0xf4, 0x11, 0x23 },  /* 3. Piano 			*/
	{0x21, 0x61, 0x1b, 0x07, 0xaf, 0x64, 0x40, 0x27 },  /* 4. Flute 			*/
	{0x22, 0x21, 0x1e, 0x06, 0xf0, 0x75, 0x08, 0x18 },  /* 5. Clarinet 			*/
	{0x31, 0x22, 0x16, 0x05, 0x90, 0x71, 0x00, 0x13 },  /* 6. Oboe 				*/
	{0x21, 0x61, 0x1d, 0x07, 0x82, 0x80, 0x10, 0x17 },  /* 7. Trumpet 			*/
	{0x23, 0x21, 0x2d, 0x16, 0xc0, 0x70, 0x07, 0x07 },  /* 8. Organ 			*/
	{0x61, 0x61, 0x1b, 0x06, 0x64, 0x65, 0x10, 0x17 },  /* 9. Horn 				*/
	{0x61, 0x61, 0x0c, 0x18, 0x85, 0xf0, 0x70, 0x07 },  /* A. Synthesizer 		*/
	{0x23, 0x01, 0x07, 0x11, 0xf0, 0xa4, 0x00, 0x22 },  /* B. Harpsichord 		*/
	{0x97, 0xc1, 0x24, 0x07, 0xff, 0xf8, 0x22, 0x12 },  /* C. Vibraphone 		*/
	{0x61, 0x10, 0x0c, 0x05, 0xf2, 0xf4, 0x40, 0x44 },  /* D. Synthesizer Bass 	*/
	{0x01, 0x01, 0x55, 0x03, 0xf3, 0x92, 0xf3, 0xf3 },  /* E. Acoustic Bass 	*/
	{0x61, 0x41, 0x89, 0x03, 0xf1, 0xf4, 0xf0, 0x13 }   /* F. Electric Guitar 	*/
};
static const int8_t pmTable[8][8] = {
		{ 0, 0, 0, 0, 0, 0, 0, 0 }, // FNUM = 000xxxxxx
		{ 0, 0, 1, 0, 0, 0,-1, 0 }, // FNUM = 001xxxxxx
		{ 0, 1, 2, 1, 0,-1,-2,-1 }, // FNUM = 010xxxxxx
		{ 0, 1, 3, 1, 0,-1,-3,-1 }, // FNUM = 011xxxxxx
		{ 0, 2, 4, 2, 0,-2,-4,-2 }, // FNUM = 100xxxxxx
		{ 0, 2, 5, 2, 0,-2,-5,-2 }, // FNUM = 101xxxxxx
		{ 0, 3, 6, 3, 0,-3,-6,-3 }, // FNUM = 110xxxxxx
		{ 0, 3, 7, 3, 0,-3,-7,-3 }, // FNUM = 111xxxxxx
	};
static uint8_t
percussions[0x3][0x8] = { /* Dumped from VRC7 */
	{0x01, 0x01, 0x18, 0x0f, 0xdf, 0xf8, 0x6a, 0x6d },  /* Base Drum 			*/
	{0x01, 0x01, 0x00, 0x00, 0xc8, 0xd8, 0xa7, 0x68 },  /* High Hat, Snare Drum */
	{0x05, 0x01, 0x00, 0x00, 0xf8, 0xaa, 0x59, 0x55 },  /* Tom-tom, Top Cymbal 	*/
},
mul[0x10] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30},
egAdvance[4][8] = {
	{ 0, 1, 0, 1, 0, 1, 0, 1 }, //  4 out of 8
	{ 0, 1, 0, 1, 1, 1, 0, 1 }, //  5 out of 8
	{ 0, 1, 1, 1, 0, 1, 1, 1 }, //  6 out of 8
	{ 0, 1, 1, 1, 1, 1, 1, 1 }, //  7 out of 8
},
keyScaleLevel[16] = { 112, 64, 48, 38, 32, 26, 22, 18, 16, 12, 10, 8, 6, 4, 2, 0 };

float ym2413_Sample, *ym2413_SampleBuffer = NULL;
static uint32_t sampleRate, originalSampleRate;
uint8_t muteControl = 0, instrumentSet[TOTAL_CHANNELS], ym2413reg, instChannels, rhythmControl, ym2413_mute;
int fmCyclesToRun = 0, ym2413_SampleCounter = 0, fmAccumulatedCycles = 0;
static int logSine[256], expTab[256], origSampleCounter = 0, bufferSize;
static uint32_t counter = 0, noise;
static Channel *channels[TOTAL_CHANNELS] = {NULL};

void setup_user_instrument(uint8_t), load_instrument(uint8_t, uint8_t*), load_rhythm(), unload_rhythm(), calculate_phase(uint8_t,uint8_t), calculate_envelope(uint8_t,uint8_t), lfsr(),
		load_instrument_0(uint8_t, uint8_t*), load_instrument_1(uint8_t, uint8_t*), load_instrument_2(uint8_t, uint8_t*), load_instrument_3(uint8_t, uint8_t*),
		load_instrument_4(uint8_t, uint8_t*), load_instrument_5(uint8_t, uint8_t*), load_instrument_6(uint8_t, uint8_t*), load_instrument_7(uint8_t, uint8_t*);
uint16_t getLogSine(uint16_t, uint8_t);
int16_t getExp(uint16_t), calculate_operator(uint8_t, uint8_t, int, int);
int calculate_attenuation(Operator *, uint8_t, int);

void init_ym2413(int buffer){
	bufferSize = buffer;
	free(ym2413_SampleBuffer);
	ym2413_SampleBuffer = malloc(bufferSize * sizeof(float));
	noise = 0x800000;
	rhythmControl = 0;
	instChannels = TOTAL_CHANNELS;
	for(int i = 0; i < TOTAL_CHANNELS; i++){//TODO: below are just hand picked sensible defaults, what are actual init values?
		free(channels[i]);
		channels[i] = malloc(sizeof(Channel));
		channels[i]->car.egState = SUSTAIN;
		channels[i]->mod.egState = SUSTAIN;
		channels[i]->keyPressed = 0;
		channels[i]->mod.p0 = channels[i]->mod.p1 = channels[i]->mod.phase = channels[i]->car.phase = channels[i]->mod.feedback = 0;
		channels[i]->mod.totalLevel = 63;
		channels[i]->mod.feedback = 0;
		channels[i]->mod.envelope = 127;
		channels[i]->car.envelope = 127;
	}
    for (int i = 0; i < 256; ++i) {
        logSine[i] = round(-log2(sin((i + 0.5) * M_PI_2 / 256.0)) * 256.0);//12bit range
        expTab[i] = round(exp2(i / 256.0) * 1024.0) - 1024;//10bit range
    }
}

void set_timings_ym2413(int div, int clock){
	originalSampleRate = sampleRate = ((uint64_t)clock << FRAC_BITS) / div;
	origSampleCounter = 0;
}

uint16_t getLogSine(uint16_t val, uint8_t wf){
// complete waveform period is 10 bit, reconstructed from an 8 bit length table
// input format: --nm pppp pppp (n=negative; m=mirror; p=phase)
//TODO: generate 10 bit length table at startup for speed
	uint16_t result = logSine[(val & 0x100) ? ((val & 0xff) ^ 0xff) : (val & 0xff)];
	if (val & 0x200) { //is negative
		if (wf)
			result = 0xfff; // zero (absolute value)
		result |= 0x8000; // negate
	}
	return result;
}

int16_t getExp(uint16_t val){
	int t = ((expTab[(val & 0xff) ^ 0xff] << 1) | 0x0800);
	int result = (t >> ((val & 0x7f00) >> 8));
	if (val & 0x8000) //is negative
		result = ~result;//ones complement
	return result;
}

void write_ym2413_register(uint8_t value){
	ym2413reg = value;
}

void write_ym2413_data(uint8_t value){
	uint8_t channel;
	switch(ym2413reg & 0xf0){
	case 0x00: /* Reg 0-7: User tone control */
		switch(ym2413reg){
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			instruments[0][ym2413reg & 7] = value;
			setup_user_instrument(ym2413reg & 0x07);
			break;
		case 0x0e: ;/* Rhytm control */
			uint8_t switched = ((rhythmControl & 0x20) ^ (value & 0x20));
			uint8_t oldKeys = rhythmControl;
			rhythmControl = (value & 0x3f);
			if(rhythmControl & 0x20){
				if(switched){// load rhythm
					instChannels = INSTRUMENT_CHANNELS;
					load_rhythm();
				}
				// set rhythm keys on/off
					if((rhythmControl & 0x01) && !(oldKeys & 0x01))//high hat
						channels[7]->mod.egState = DAMP;
					if((rhythmControl & 0x02) && !(oldKeys & 0x02))//top cymbal
						channels[8]->car.egState = DAMP;
					if((rhythmControl & 0x04) && !(oldKeys & 0x04))//tom tom
						channels[8]->mod.egState = DAMP;
					if((rhythmControl & 0x08) && !(oldKeys & 0x08))//snare drum
						channels[7]->car.egState = DAMP;
					if((rhythmControl & 0x10) && !(oldKeys & 0x10)){//base drum
						channels[6]->car.egState = DAMP;
						channels[6]->mod.egState = DAMP;
						channels[6]->keyPressed = (rhythmControl & 0x10);
					}
			}
			else if(switched){ // unload rhythm
				instChannels = TOTAL_CHANNELS;
				unload_rhythm();
			}
			break;
		}
		break;
	case 0x10: /* Reg 10-18: F-number (low bits) */
		channel = ((ym2413reg & 0xf) % 9);
		channels[channel]->fNumber = ((channels[channel]->fNumber & 0x0100) | value);
		break;
	case 0x20: /* Reg 20-28: Block/F-number; key; sustain mode */
		channel = ((ym2413reg & 0xf) % 9);
		channels[channel]->fNumber = ((channels[channel]->fNumber & 0x00ff) | ((value & 0x01) << 8));
		channels[channel]->keyCode = (value & 0x07);
		channels[channel]->block = ((value & 0x0e) >> 1);
		if((channels[channel]->keyPressed ^ (value & 0x10)) && !channels[channel]->keyPressed){
			channels[channel]->car.egState = DAMP;
			channels[channel]->mod.egState = DAMP;
		}
		channels[channel]->keyPressed = (value & 0x10);
		channels[channel]->sustainMode = (value & 0x20);
		break;
	case 0x30: /* Reg 30-38: Instrument selection and volume */
		channel = ((ym2413reg & 0xf) % 9);
		instrumentSet[channel] = value;
		channels[channel]->vol = (value & 0x0f);// -3dB per step
		if(channel < instChannels)
			load_instrument(channel, &instruments[value>>4][0]);
		break;
	}
}

void load_rhythm(){
	load_instrument(6, percussions[0]); //bass drum
	load_instrument(7, percussions[1]); //high hat and snare drum
	load_instrument(8, percussions[2]); //tom-tom and cymbal
}

void unload_rhythm(){
	load_instrument(6, instruments[instrumentSet[6] >> 4]);
	load_instrument(7, instruments[instrumentSet[7] >> 4]);
	load_instrument(8, instruments[instrumentSet[8] >> 4]);
	//should total levels be set (see mame)?
}

void setup_user_instrument(uint8_t value){
uint8_t *instrument = instruments[0];
switch(value){
case 0:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_0(channel, instrument);
		}
	}
	break;
case 1:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_1(channel, instrument);
		}
	}
	break;
case 2:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_2(channel, instrument);
		}
	}
	break;
case 3:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_3(channel, instrument);
		}
	}
	break;
case 4:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_4(channel, instrument);
		}
	}
	break;
case 5:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_5(channel, instrument);
		}
	}
	break;
case 6:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_6(channel, instrument);
		}
	}
	break;
case 7:
	for(uint8_t channel = 0;channel<instChannels;channel++){
		if(!(instrumentSet[channel] & 0xf0)){
			load_instrument_7(channel, instrument);
		}
	}
	break;
}
}

void load_instrument_0(uint8_t channel, uint8_t *instrument){
	channels[channel]->mod.multi = mul[instrument[0] & 0x0f];
	channels[channel]->mod.ksrShift = instrument[0] & 0x10;
	channels[channel]->mod.egType = instrument[0] & 0x20;
	channels[channel]->mod.vibrato = instrument[0] & 0x40;
	channels[channel]->mod.am = instrument[0] & 0x80;
}
void load_instrument_1(uint8_t channel, uint8_t *instrument){
	channels[channel]->car.multi = mul[instrument[1] & 0x0f];
	channels[channel]->car.ksrShift = instrument[1] & 0x10;
	channels[channel]->car.egType = instrument[1] & 0x20;
	channels[channel]->car.vibrato = instrument[1] & 0x40;
	channels[channel]->car.am = instrument[1] & 0x80;
}
void load_instrument_2(uint8_t channel, uint8_t *instrument){
	channels[channel]->mod.ksl = instrument[2] >> 6;
	channels[channel]->mod.totalLevel = instrument[2] & 0x3f; /* amount of modulation */
}
void load_instrument_3(uint8_t channel, uint8_t *instrument){
	channels[channel]->car.ksl = instrument[3] >> 6;
	channels[channel]->mod.wave = (instrument[3] & 0x08) ? 1 : 0;
	channels[channel]->car.wave = (instrument[3] & 0x10) ? 1 : 0;
	channels[channel]->mod.feedback = (instrument[3] & 0x07);
}
void load_instrument_4(uint8_t channel, uint8_t *instrument){
	channels[channel]->mod.attackRate = (instrument[4] >> 4);
	channels[channel]->mod.decayRate = (instrument[4] & 0xf);
}
void load_instrument_5(uint8_t channel, uint8_t *instrument){
	channels[channel]->car.attackRate = (instrument[5] >> 4);
	channels[channel]->car.decayRate = (instrument[5] & 0xf);
}
void load_instrument_6(uint8_t channel, uint8_t *instrument){
	channels[channel]->mod.sustainLevel = (instrument[6] >> 4);
	channels[channel]->mod.releaseRate = (instrument[6] & 0xf);
}
void load_instrument_7(uint8_t channel, uint8_t *instrument){
	channels[channel]->car.sustainLevel = (instrument[7] >> 4);
	channels[channel]->car.releaseRate = (instrument[7] & 0xf);
}

void load_instrument(uint8_t channel, uint8_t *instrument){
	load_instrument_0(channel, instrument);
	load_instrument_1(channel, instrument);
	load_instrument_2(channel, instrument);
	load_instrument_3(channel, instrument);
	load_instrument_4(channel, instrument);
	load_instrument_5(channel, instrument);
	load_instrument_6(channel, instrument);
	load_instrument_7(channel, instrument);
}

void lfsr(){
		if (noise & 1)
			noise ^= 0x800302;
		noise >>= 1;
}

void calculate_envelope(uint8_t channel, uint8_t isCarrier){
uint8_t isRhythm = 0;
if((rhythmControl & 0x20) && channel >=7)
		isRhythm = 1;
	//EG step size is -.375dB (-48/128)
Operator *op = isCarrier ? &channels[channel]->car : &channels[channel]->mod;
uint8_t keyOn = channels[channel]->keyPressed;
if(isRhythm){
	switch(channel << isCarrier){
	case 7://high hat
		keyOn = (rhythmControl & 0x01);
		break;
	case 14://snare drum
		keyOn = (rhythmControl & 0x08);
		break;
	case 8://tom tom
		keyOn = (rhythmControl & 0x04);
		break;
	case 16://top cymbal
		keyOn = (rhythmControl & 0x02);
		break;
	}
}

uint8_t isMax = (op->envelope >> 2) == ATTENUATION_SILENT >> 2;

/* update EG phase */
if(op->egState == DAMP && isMax && (isRhythm || isCarrier)){
	//should this be handled below, when advancing the envelope based on rate?
	if(op->attackRate == 15){
		op->egState = DECAY;
		op->envelope = ATTENUATION_MIN;	//otherwise it will be silent
		if(!isRhythm){
			channels[channel]->mod.envelope = ATTENUATION_MIN;
		}
	}
	else
		op->egState = ATTACK;
	op->phase = 0;
	if(!isRhythm){
		channels[channel]->mod.egState = op->egState;
		channels[channel]->mod.phase = op->phase;
	}
} else if(op->egState == ATTACK && op->envelope == ATTENUATION_MIN)
	op->egState = DECAY;
  else if(op->egState == DECAY && (op->envelope >> 3) == op->sustainLevel)
	op->egState = SUSTAIN;

/* get rate */
uint8_t baseRate;
uint8_t isAttack;
if(!keyOn && (isRhythm || isCarrier)){// only for carrier AND rhythm ops
	if(op->egType)
		baseRate = op->releaseRate;
	else if(channels[channel]->sustainMode)
		baseRate = 5; //RS
	else
		baseRate = 7; //RR'
	isAttack = 0;
} else{
	switch(op->egState){
	case DAMP:
		baseRate = 12;
		isAttack = 0;
		break;
	case ATTACK:
		baseRate = op->attackRate;
		isAttack = 1;
		break;
	case DECAY:
		baseRate = op->decayRate;
		isAttack = 0;
		break;
	case SUSTAIN:
		baseRate = op->egType ? 0 : op->releaseRate; // is this correct for rhythm?
		isAttack = 0;
		break;
	}
}
//Effective rate is based on Table III-2 and formula [RATE = 4 x R + Rks] in manual:
uint8_t key = ((channels[channel]->block << 1) | (channels[channel]->fNumber >> 8));
uint8_t rate = (((baseRate << 2) + (op->ksr ? key : (key >> 2))));//0-63
if(rate > RATE_MAX)
	rate = RATE_MAX;
uint8_t *egStep = egAdvance[rate & 3];
if(isAttack){
	switch(rate >> 2){
	//very slow and fast rates give infinite envelope:
	case 0:
	case 15:
		op->envelope = ATTENUATION_MIN; // corner case: organ modulator stuck in ATTACK (mod attack = 15 but car attack = 7)
		break;
	//special case for fast rates:
	case 12://rate 48+
	case 13://rate 52+
	case 14: ;//rate 56+
		int m = (16 - (rate >> 2));
		m -= (egStep[counter & 0xc] >> 1);
		op->envelope = (op->envelope - (op->envelope >> m) - 1);
		break;
	//normal behavior:
	default: ;
		uint8_t shift = (13 - (rate >> 2));
		int mask = (((1 << shift) -1) & ~3);
		if(!(counter & mask)){
			if(egStep[(counter >> shift) & 7])
				op->envelope = (op->envelope - (op->envelope >> 4) - 1);
		}
		break;
	}
	if(op->envelope < ATTENUATION_MIN){
		op->envelope = ATTENUATION_MIN;
	}
} else{
	switch(rate >> 2){
	//exceptions for very slow or fast rates:
	case 0://rates 0-3 gives infinite envelope
		break;
	case 13://rates 52+
		op->envelope += (egStep[((counter & 0xc) >> 1) | (counter & 1)] + 1);
		break;
	case 14://rates 56+
		op->envelope += (egStep[(counter & 0xc) >> 1] + 1);
		break;
	case 15://rates 60+
		op->envelope += 2;
		break;
	//normal behavior:
	default: ;
		uint8_t shift = 13 - (rate / 4);
		int mask = (1 << shift) - 1;
		if (!(counter & mask))
			op->envelope += egStep[(counter >> shift) & 7];
		break;
	}
	if(op->envelope > ATTENUATION_MAX)
		op->envelope = ATTENUATION_MAX;
}
}

void calculate_phase(uint8_t channel, uint8_t isCarrier){
	int vib = 0;
	Operator *op = isCarrier ? &channels[channel]->car : &channels[channel]->mod;
	if(op->vibrato)
		vib = pmTable[channels[channel]->fNumber >> 6][(counter >> 10) & 7]; // update only when written to?

	//the formula: (((2fnum + pmLFO) * mlTab[ML]) << block) / 4
	//the result of this phase calculation is 10.9 fixed point values:
	op->phaseIncrement = (((((channels[channel]->fNumber << 1) + vib) * op->multi) << channels[channel]->block) >> 2);
	op->phase += op->phaseIncrement;
}

static const uint8_t amTable[210] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
   10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,
   12,12,12,12,12,12,12,12,13,13,13,
   12,12,12,12,12,12,12,12,11,11,11,11,11,11,11,11,
   10,10,10,10,10,10,10,10, 9, 9, 9, 9, 9, 9, 9, 9,
    8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7,
    6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0 };

int16_t calculate_operator(uint8_t channel, uint8_t isCarrier, int phase, int level){
	Operator *op = isCarrier ? &channels[channel]->car : &channels[channel]->mod;
	calculate_envelope(channel, isCarrier);
	calculate_phase(channel, isCarrier);
	if((op->envelope & ATTENUATION_SILENT) == ATTENUATION_SILENT)
		return 0;
	int attenuation = calculate_attenuation(op,channel,level);
	int p = (op->phase >> 9) + phase;
	return getExp(getLogSine(p, op->wave) + (attenuation << 4));
}

int calculate_attenuation(Operator *op, uint8_t channel, int level){
	int ksl = 0, am = 0;
	if(op->am)
		am = amTable[(counter >> 6) % 210];
	if(op->ksl){
		ksl = (((channels[channel]->block << 4) - keyScaleLevel[channels[channel]->fNumber >> 5]) >> (3 - op->ksl));
		if(ksl < 0)
			ksl = 0;
	}
	int attenuation = (op->envelope + level + ksl + am);
	if(attenuation > ATTENUATION_MAX)
		attenuation = ATTENUATION_MAX;
	return attenuation;
}

void run_ym2413(){
	/* calculate phase, calculate envelope, calculate attenuation, output sample */
	int16_t tmp_sample = 0;
	int16_t m;
	uint8_t sdBit, hhRes;
	uint16_t phase;
	while(fmCyclesToRun){
		lfsr();
		for(uint8_t channel = 0;channel < instChannels;channel++){
			m = (calculate_operator(channel, 0, (channels[channel]->mod.feedback ? ((channels[channel]->mod.p0 + channels[channel]->mod.p1) >> (8-channels[channel]->mod.feedback)) : 0), channels[channel]->mod.totalLevel << 1) >> 1);
			channels[channel]->mod.p1 = channels[channel]->mod.p0;
			channels[channel]->mod.p0 = m;
			//output is 9bit (-255 - 255) so we shift right 4 (is the output signed now?)
			if(channelMask & (1 << channel))
				tmp_sample += (calculate_operator(channel, 1, (m << 1), (channels[channel]->vol << 3)) >> 4);
		}
			if(rhythmControl & 0x20){
				//base drum
				m = (calculate_operator(6, 0, (channels[6]->mod.feedback ? ((channels[6]->mod.p0 + channels[6]->mod.p1) >> (8-channels[6]->mod.feedback)) : 0), channels[6]->mod.totalLevel << 1) >> 1);
				channels[6]->mod.p1 = channels[6]->mod.p0;
				channels[6]->mod.p0 = m;
				//should be twice normal output?
				if(rhythmMask & (1 << 0))
				tmp_sample += (calculate_operator(6, 1, (m << 1), (channels[6]->vol << 3)) >> 3);

				//hi hat
				hhRes = (((channels[8]->car.phase >> 9) >> 5) | ((channels[8]->car.phase >> 9) >> 3)) & 1;
				if (hhRes)
					phase = (0x200|(0xd0>>2));
				else{
					hhRes = ((((channels[7]->mod.phase >> 9) >> 7) ^ ((channels[7]->mod.phase >> 9) >> 2)) | ((channels[7]->mod.phase >> 9) >> 3)) & 1;
					phase = (hhRes ? (0x200|(0xd0>>2)) : 0xd0);
				}
				if ((phase & 0x200) && noise)
						phase = 0x200|0xd0;
				else if (noise)
						phase = 0xd0>>2;
				if(rhythmMask & (1 << 1))
				tmp_sample += (calculate_operator(7, 0, phase, (instrumentSet[7] & 0xf0) >> 1) >> 3);

				//snare drum
				//get base freq from hi hat channel:
				sdBit = (((channels[7]->mod.phase >> 9) >> 8) & 1);
				phase = sdBit ? 0x200 : 0x100;
				if (noise & 1)
					phase ^= 0x100;
				if(rhythmMask & (1 << 2))
				tmp_sample += (calculate_operator(7, 1, phase, (instrumentSet[7] & 0x0f) << 3) >> 3);

				//tom tom
				if(rhythmMask & (1 << 3))
				tmp_sample += (calculate_operator(8, 0, 0, (instrumentSet[8] & 0xf0) >> 1) >> 3);

				//top cymbal
				hhRes = (((channels[8]->car.phase >> 9) >> 5) | ((channels[8]->car.phase >> 9) >> 3)) & 1;
				if (hhRes)
					phase = 0x300;
				else{
					hhRes = ((((channels[7]->mod.phase >> 9) >> 7) ^ ((channels[7]->mod.phase >> 9) >> 2)) | ((channels[7]->mod.phase >> 9) >> 3)) & 1;
					phase = (hhRes ? 0x300 : 0x100);
				}
				if(rhythmMask & (1 << 4))
				tmp_sample += (calculate_operator(8, 1, phase, (instrumentSet[8] & 0x0f) << 3) >> 3);
				}
			if(!ym2413_mute)
				ym2413_Sample += tmp_sample; //global sample accumulator instead
			else
				ym2413_Sample += 0;
			tmp_sample = 0; // move out
			origSampleCounter++; // make global

		//move all of this out of here:
		if(origSampleCounter >= (sampleRate >> FRAC_BITS)){// can this be simplified with fixed point?
			ym2413_SampleBuffer[ym2413_SampleCounter++] = (ym2413_Sample / (origSampleCounter * TOTAL_CHANNELS * 127));
			if(ym2413_SampleCounter == bufferSize){
				output_sound(ym2413_SampleBuffer, ym2413_SampleCounter);
				ym2413_SampleCounter = 0;
			}
			sampleRate = originalSampleRate + sampleRate - (origSampleCounter << FRAC_BITS);
			ym2413_Sample = 0;
			origSampleCounter = 0;
		}
		counter++;
		fmCyclesToRun--;
	}
}
