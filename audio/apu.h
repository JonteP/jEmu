#ifndef APU_H_
#define APU_H_

#define CHANNELS 1
#define BUFFER_SIZE (8192)
#define CPU_CLOCK 1789773 /*1789773 (official?); 1786831*/

#include <stdint.h>
extern uint_fast8_t apuStatus, apuFrameCounter, pulse1Length, pulse2Length,
		pulse1Control, pulse2Control, sweep1Divide, sweep1Reload, sweep1Shift, sweep1,
		sweep2Divide, sweep2Reload, sweep2Shift, sweep2, env1Start, env2Start, envNoiseStart,
		env1Divide, env2Divide, envNoiseDivide, triLength, triLinReload, triControl,
		noiseLength, noiseMode, noiseControl, dmcOutput, dmcControl, dmcInt, frameInt, frameWriteDelay, frameWrite, dmcRestart, dmcSilence;
extern int8_t pulse1Duty, pulse2Duty;
extern int_fast16_t pulse2Timer, pulse1Timer;
extern uint_fast16_t noiseTable[0x10], rateTable[0x10], triTimer, noiseShift, noiseTimer, dmcRate, dmcAddress, dmcCurAdd, dmcLength, dmcBytesLeft, sampleCounter, dmcTemp;
extern uint_fast8_t lengthTable[0x20];
extern uint32_t frameIrqDelay, apucc, frameIrqTime;
extern float sampleBuffer[BUFFER_SIZE], sampleRate, originalSampleRate;
extern const int samplesPerSecond;
void run_apu(uint_fast16_t), dmc_fill_buffer(void), quarter_frame(void), half_frame(void);

#endif /* APU_H_ */
