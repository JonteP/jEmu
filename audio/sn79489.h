#ifndef SN79489_H_
#define SN79489_H_

#include <stdio.h>
#include <stdint.h>

void init_sn79489(int), reset_sn79489(void), close_sn79489(void), write_sn79489(uint8_t), run_sn79489(void), set_timings_sn79489(int, int);

struct ToneChannel {
	uint16_t reg;
	uint16_t counter;
	uint8_t phase;
	uint8_t volume;
	float output;
};

extern uint8_t sn79489_mute;
extern float fps;
extern float *sn79489_SampleBuffer;
extern int psgAccumulatedCycles, sn79489_SampleCounter, audioCyclesToRun;

#endif /* SN79489_H_ */
