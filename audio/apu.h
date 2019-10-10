#ifndef APU_H_
#define APU_H_
#include <stdint.h>

typedef enum apu_version {
	APU_NTSC = 0,
	APU_PAL	= 1
} APU_Version;

extern uint_fast8_t apuStatus, apuFrameCounter, pulse1Length, pulse2Length,
		pulse1Control, pulse2Control, sweep1Divide, sweep1Reload, sweep1Shift, sweep1,
		sweep2Divide, sweep2Reload, sweep2Shift, sweep2, env1Start, env2Start, envNoiseStart,
		env1Divide, env2Divide, envNoiseDivide, triLength, triLinReload, triControl,
		noiseLength, noiseMode, noiseControl, dmcOutput, dmcControl, dmcInt, frameInt, frameWriteDelay, frameWrite, dmcRestart, dmcSilence;
extern int8_t pulse1Duty, pulse2Duty;
extern int_fast16_t pulse2Timer, pulse1Timer;
extern uint_fast16_t noiseTable[0x10], dmcRateTable[0x02][0x10], triTimer, noiseShift, noiseTimer, dmcRate, dmcAddress, dmcCurAdd, dmcLength, dmcBytesLeft, dmcTemp;
extern uint_fast8_t lengthTable[0x20];
extern uint32_t frameIrqDelay, apucc, frameIrqTime;
extern const int samplesPerSecond;
void run_apu(uint16_t), dmc_fill_buffer(void), quarter_frame(void), half_frame(void), init_apu(int), set_timings_apu(int, int);

#endif /* APU_H_ */
