#ifndef YM2413_H_
#define YM2413_H_

#include <stdint.h>
extern uint8_t muteControl;

enum state{
	DAMP = 0,
	ATTACK = 1,
	DECAY = 2,
	SUSTAIN = 3,
};
typedef struct operator{
	uint8_t multi;
	uint8_t ksrShift;
	uint8_t egType;
	uint8_t vibrato;
	uint8_t am;
	uint8_t ksl;
	uint8_t ksr;
	uint8_t wave;
	uint32_t phase;
	uint32_t phaseIncrement; /* 10.9 fixed point */

	// ENVELOPE GENERATOR
	uint8_t attackRate;
	uint8_t decayRate;
	uint8_t sustainLevel;
	uint8_t releaseRate;
	enum state egState;
	int16_t envelope;

	uint8_t totalLevel; // mod only
	uint8_t feedback; // mod only
	int16_t p0; //mod only
	int16_t p1; //mod only
}Operator;

typedef struct channel{
	uint8_t keyPressed;
	uint8_t sustainMode;
	uint16_t fNumber;
	uint8_t block;
	uint8_t keyCode;
	uint8_t vol;
	uint8_t instr;
	Operator mod;
	Operator car;
} Channel;

extern uint8_t ym2413_mute;
extern int fmCyclesToRun, ym2413_SampleCounter, fmAccumulatedCycles;
extern float *ym2413_SampleBuffer;
void write_ym2413_register(uint8_t), write_ym2413_data(uint8_t), run_ym2413(void), init_ym2413(int), set_timings_ym2413(int, int);

#endif
