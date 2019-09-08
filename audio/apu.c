#include "apu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "globals.h"
#include "SDL.h"
#include "6502.h"
#include "my_sdl.h"
#include "ppu.h"
#include "mapper.h"
						/* 	shifted up by 1 to work */
static uint_fast16_t frameClock[5] = {7457, 14913, 22371, 29829, 37281};
static uint_fast16_t frameReset[2] = {29830, 37282};

static float pulse_table[31] = { 0.0116, 0.0229, 0.0340, 0.0448, 0.0554, 0.0657, 0.0757, 0.0856, 0.0952, 0.1046, 0.1139, 0.1229, 0.1317,
						   0.1404, 0.1488, 0.1571, 0.1652, 0.1732, 0.1810, 0.1886, 0.1961, 0.2035, 0.2107, 0.2178, 0.2247, 0.2315,
						   0.2382, 0.2447, 0.2512, 0.2575, 0.2637 };
static float tnd_table[203] =  { 0.0067, 0.0133, 0.0199, 0.0265, 0.0330, 0.0394, 0.0458, 0.0521, 0.0584, 0.0646, 0.0708, 0.0769, 0.0830,
						   0.0891, 0.0951, 0.1010, 0.1069, 0.1128, 0.1186, 0.1243, 0.1300, 0.1357, 0.1414, 0.1470, 0.1525, 0.1580,
						   0.1635, 0.1689, 0.1743, 0.1797, 0.1850, 0.1903, 0.1955, 0.2007, 0.2058, 0.2110, 0.2161, 0.2211, 0.2261,
						   0.2311, 0.2360, 0.2410, 0.2458, 0.2507, 0.2555, 0.2603, 0.2650, 0.2697, 0.2744, 0.2790, 0.2836, 0.2882,
						   0.2928, 0.2973, 0.3018, 0.3062, 0.3107, 0.3151, 0.3194, 0.3238, 0.3281, 0.3324, 0.3366, 0.3409, 0.3451,
						   0.3493, 0.3534, 0.3575, 0.3616, 0.3657, 0.3697, 0.3738, 0.3778, 0.3817, 0.3857, 0.3896, 0.3935, 0.3973,
						   0.4012, 0.4050, 0.4088, 0.4126, 0.4163, 0.4201, 0.4238, 0.4275, 0.4311, 0.4348, 0.4384, 0.4420, 0.4455,
						   0.4491, 0.4526, 0.4561, 0.4596, 0.4631, 0.4665, 0.4700, 0.4734, 0.4768, 0.4801, 0.4835, 0.4868, 0.4901,
						   0.4934, 0.4967, 0.4999, 0.5032, 0.5064, 0.5096, 0.5128, 0.5159, 0.5191, 0.5222, 0.5253, 0.5284, 0.5315,
						   0.5346, 0.5376, 0.5406, 0.5436, 0.5466, 0.5496, 0.5526, 0.5555, 0.5584, 0.5613, 0.5642, 0.5671, 0.5700,
						   0.5728, 0.5757, 0.5785, 0.5813, 0.5841, 0.5869, 0.5896, 0.5924, 0.5951, 0.5978, 0.6005, 0.6032, 0.6059,
						   0.6085, 0.6112, 0.6138, 0.6165, 0.6191, 0.6217, 0.6242, 0.6268, 0.6294, 0.6319, 0.6344, 0.6369, 0.6394,
						   0.6419, 0.6444, 0.6469, 0.6493, 0.6518, 0.6542, 0.6566, 0.6590, 0.6614, 0.6638, 0.6662, 0.6685, 0.6709,
						   0.6732, 0.6756, 0.6779, 0.6802, 0.6825, 0.6847, 0.6870, 0.6893, 0.6915, 0.6938, 0.6960, 0.6982, 0.7004,
						   0.7026, 0.7048, 0.7070, 0.7091, 0.7113, 0.7134, 0.7156, 0.7177, 0.7198, 0.7219, 0.7240, 0.7261, 0.7282,
						   0.7303, 0.7323, 0.7344, 0.7364, 0.7384, 0.7405, 0.7425, 0.7445 };
static float pulseMixSample = 0, tndMixSample = 0, expSample = 0;
static uint_fast8_t frameCounter = 0, sweep1Counter = 0, sweep2Counter = 0, env1Decay = 0, env2Decay = 0, envNoiseDecay = 0,
					triLinear = 0, dmcBitsLeft = 8, dmcShift = 0, pulse1Mute = 0, pulse2Mute = 0, sCount = 0;
static int_fast8_t triSeq = 0, triBuff = 0;
static uint_fast16_t triTemp, noiseTemp, framecc = 0, pulse1Sample = 0, pulse2Sample = 0, triSample = 0, tmpcnt = 0, noiseSample = 0;
static int16_t pulse1Temp = 0, pulse2Temp = 0, pulse1Change = 0, pulse2Change = 0;

uint_fast8_t apuStatus, apuFrameCounter, pulse1Length = 0, pulse2Length = 0, pulse1Control = 0, pulse2Control = 0,
		     sweep1Divide = 0, sweep1Reload = 0, env1Start = 0, env2Start = 0, envNoiseStart = 0, env1Divide = 0,
			 env2Divide = 0, envNoiseDivide = 0, sweep1Shift = 0, sweep1 = 0, sweep2Divide = 0, sweep2Shift = 0, sweep2 = 0,
			 sweep2Reload = 0, triLength = 0, triLinReload = 0, triControl, noiseControl, noiseLength = 0, noiseMode = 0,
			 dmcOutput = 0, dmcControl = 0, dmcSilence = 1, dmcRestart = 0;
int_fast8_t pulse1Duty = 0, pulse2Duty = 0;
int_fast16_t pulse2Timer = 0, pulse1Timer = 0;
uint_fast16_t sampleCounter = 0, triTimer = 0, noiseShift, noiseTimer,
		dmcRate = 0, dmcAddress, dmcCurAdd, dmcLength = 0, dmcBytesLeft = 0, dmcTemp;
float sampleBuffer[BUFFER_SIZE] = {0};
uint32_t apucc = 0;

float sampleRate, originalSampleRate;
const int samplesPerSecond = 48000;
uint_fast8_t lengthTable[0x20] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14,
		12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32,
		30 };
static uint_fast8_t dutySequence[4][8] = { { 0, 0, 0, 0, 0, 0, 0, 1 },
							   { 0, 0, 0, 0, 0, 0, 1, 1 },
							   { 0, 0, 0, 0, 1, 1, 1, 1 },
							   { 1, 1, 1, 1, 1, 1, 0, 0 } };
static uint_fast8_t triSequence[0x20] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
uint_fast16_t noiseTable[0x10] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};
uint_fast16_t rateTable[0x10] = { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54 };
uint_fast8_t dmcInt = 0, frameInt = 0, frameWriteDelay, frameWrite = 0;
static int samplesPerSec = 0;

void run_apu(uint_fast16_t ntimes) { /* apu cycle times */
	while (ntimes) {
		if (frameWrite) {
			if (!frameWriteDelay) {
				frameCounter = 0;
				framecc = 0;
				if (apuFrameCounter&0x40) {
					frameInt = 0;
				}
				if (apuFrameCounter&0x80) {
					quarter_frame();
					half_frame();
				}
				frameWrite = 0;
			} else
				frameWriteDelay--;
		}
		irq_cpu_clocked();

		if (dmcInt || frameInt) {
			irqPulled = 1;
		}

		if (framecc == frameClock[frameCounter]) {
			if ((!(apuFrameCounter&0x80)) || ((apuFrameCounter&0x80) && frameCounter != 3)) {
				quarter_frame();
			}
			if ((apuFrameCounter & 0x80) && (frameCounter == 1 || frameCounter == 4)) {
				half_frame();
		  } else if ((!(apuFrameCounter & 0x80)) && (frameCounter%2)) {
				half_frame();
		  }
			frameCounter++;
		}
		if ((!(apuFrameCounter&0xc0)) && framecc >= 29828 && framecc <= 29830) {
			frameInt = 1;
		}
		if (framecc == frameReset[(apuFrameCounter & 0x80)>>7]) {
			frameCounter = 0;
			framecc = 0;
		}
		pulse1Change = (sweep1&8) ? -((pulse1Timer>>sweep1Shift)+1) : (pulse1Timer>>sweep1Shift);
		pulse2Change = (sweep2&8) ? -(pulse2Timer>>sweep2Shift) : (pulse2Timer>>sweep2Shift);
		if ((pulse1Change + pulse1Timer) > 0x7ff || pulse1Timer < 8)
			pulse1Mute = 1;
		else
			pulse1Mute = 0;

		if ((pulse2Change + pulse2Timer) > 0x7ff || pulse2Timer < 8)
			pulse2Mute = 1;
		else
			pulse2Mute = 0;

		if (pulse1Length && (apuStatus&1)) {
			if (pulse1Timer >= 8 && !pulse1Mute) {
				pulse1Sample = (pulse1Control&0x10) ? (dutySequence[(pulse1Control>>6)&3][pulse1Duty>>1] * pulse1Control&0xf) : (dutySequence[(pulse1Control>>6)&3][pulse1Duty>>1] * env1Decay);
			} else
				pulse1Sample = 0;
			if (pulse1Temp < 0) {
				pulse1Temp = pulse1Timer;
				pulse1Duty--;
				if (pulse1Duty < 0)
					pulse1Duty = 15;
			}
			pulse1Temp--;
		} else
			pulse1Sample = 0;

		if (pulse2Length && (apuStatus&2)) {
			if (pulse2Timer >= 8 && !pulse2Mute) {
				pulse2Sample = (pulse2Control&0x10) ? (dutySequence[(pulse2Control>>6)&3][pulse2Duty>>1] * pulse2Control&0xf) : (dutySequence[(pulse2Control>>6)&3][pulse2Duty>>1] * env2Decay);
			} else
				pulse2Sample = 0;
			if (pulse2Temp < 0) {
				pulse2Temp = pulse2Timer;
				pulse2Duty--;
				if (pulse2Duty < 0)
					pulse2Duty = 15;
			}
			pulse2Temp--;
		} else
			pulse2Sample = 0;

		pulseMixSample += pulse_table[pulse1Sample+pulse2Sample];

		if (triLength && triLinear && (apuStatus&4)) {
				triSample = triSequence[triSeq];
				triBuff = triSample;
			if (!triTemp) {
				triTemp = triTimer;
				triSeq--;
				if (triSeq < 0)
					triSeq = 31;
			}
			triTemp--;
		} else
			triSample = triBuff;

		if (noiseLength && (apuStatus&8)) {
			if (!(noiseShift&1)) {
				noiseSample = (noiseControl&0x10) ? (noiseControl&0xf) : envNoiseDecay;
			} else
				noiseSample = 0;
			if (!noiseTemp) {
				noiseTemp = noiseTimer;
				noiseShift = ((noiseShift>>1) | ((noiseMode ? ((noiseShift&1) ^ ((noiseShift>>1)&1)) : ((noiseShift&1) ^ ((noiseShift>>1)&1)))<<14));
			}
			else if (cpucc%2)
				noiseTemp--;
		} else
			noiseSample = 0;

		if (dmcRestart) {
			dmcRestart = 0;
			dmcBytesLeft = dmcLength;
			dmcCurAdd = dmcAddress;
		}
			if (!dmcTemp) {
				dmcTemp = dmcRate;
				if (!dmcBitsLeft) {
					if (dmcBytesLeft) {
						dmcBitsLeft = 7;
						dmc_fill_buffer();
						dmcSilence = 0;
					} else
						dmcSilence = 1;
				} else
					dmcBitsLeft--;
				if (!dmcSilence) {
					if (!((dmcOutput + ((dmcShift&1) ? 2 : -2)) & 0x80))
						dmcOutput += ((dmcShift&1) ? 2 : -2);
				}
				dmcShift = (dmcShift>>1);
			}
			dmcTemp--;

		tndMixSample += tnd_table[3 * triSample + 2 * noiseSample + dmcOutput];

		if (expSound)
			expSample += (expansion_sound() / 60);

		tmpcnt++;
		if (tmpcnt==(int)sampleRate) {
			sCount++;
			samplesPerSec++;
			if (sCount==7)
				sCount = 0;
			if (expSound)
				sampleBuffer[sampleCounter++] = ((pulseMixSample/tmpcnt) + (tndMixSample/tmpcnt) + (expSample/tmpcnt)) / 2;
			else
				sampleBuffer[sampleCounter++] = (pulseMixSample/tmpcnt) + (tndMixSample/tmpcnt);
			if (sampleCounter == BUFFER_SIZE)
				output_sound();
			pulseMixSample = 0;
			tndMixSample = 0;
			expSample = 0;
			sampleRate = (float)(originalSampleRate + sampleRate - tmpcnt);
			tmpcnt = 0;
		}
		ntimes--;
		apu_wait--;
		framecc++;
		if (apucc == cpuClock)
		{
			apucc = 0;
			samplesPerSec = 0;
		}
		apucc++;
	}
}

void half_frame () {
	if (pulse1Length) {
		if (!((pulse1Control>>5)&1)) {
			pulse1Length--;
		}
	}
	if (pulse2Length) {
		if (!((pulse2Control>>5)&1))
			pulse2Length--;
	}
	if (triLength) {
		if (!((triControl>>7)&1))
			triLength--;
	}
	if (noiseLength) {
		if (!((noiseControl>>5)&1))
			noiseLength--;
	}

	if (!sweep1Counter && (sweep1&0x80) && !pulse1Mute && sweep1Shift) {
		pulse1Timer += pulse1Change;
	}
	if (!sweep1Counter || sweep1Reload) {
		sweep1Counter = sweep1Divide;
		sweep1Reload = 0;
	}
	 else
		sweep1Counter--;

	if (!sweep2Counter && (sweep2&0x80) && !pulse2Mute && sweep2Shift) {
		pulse2Timer += pulse2Change;
	}
	if (!sweep2Counter || sweep2Reload) {
		sweep2Counter = sweep2Divide;
		sweep2Reload = 0;
	} else
		sweep2Counter--;


}

void dmc_fill_buffer () {

		dmcSilence = 0;
	/* TODO: stall CPU */
	/*	cpuStall = 1;
		apu_wait += 6;
		ppu_wait += (6*3); */
		dmcShift = cpuread(dmcCurAdd);
		if (dmcCurAdd == 0xffff)
			dmcCurAdd = 0x8000;
		else
			dmcCurAdd++;
		dmcBytesLeft--;
		if (!dmcBytesLeft && (dmcControl&0x40)) {
			dmcBytesLeft = dmcLength;
			dmcCurAdd = dmcAddress;
	}	else if (!dmcBytesLeft && (dmcControl&0x80)) {
		dmcInt = 1;
	}
}


void quarter_frame () {
	if (env1Start) {
		env1Start = 0;
		env1Decay = 15;
		env1Divide = (pulse1Control&0xf);
	} else {
		if (!env1Divide) {
			env1Divide = (pulse1Control&0xf);
				if (!env1Decay && (pulse1Control&0x20)) {
					env1Decay = 15;
				} else if (env1Decay)
					env1Decay--;
		} else
			env1Divide--;
	}
	if (env2Start) {
		env2Start = 0;
		env2Decay = 15;
		env2Divide = (pulse2Control&0xf);
	} else {
		if (!env2Divide) {
			env2Divide = (pulse2Control&0xf);
				if (!env2Decay && (pulse2Control&0x20)) {
					env2Decay = 15;
				} else if (env2Decay)
					env2Decay--;
		} else
			env2Divide--;
	}
	if (envNoiseStart) {
		envNoiseStart = 0;
		envNoiseDecay = 15;
		envNoiseDivide = (noiseControl&0xf);
	} else {
		if (!envNoiseDivide) {
			envNoiseDivide = (noiseControl&0xf);
				if (!envNoiseDecay && (noiseControl&0x20)) {
					envNoiseDecay = 15;
				} else if (envNoiseDecay)
					envNoiseDecay--;
		} else
			envNoiseDivide--;
	}

	if (triLinReload)
		triLinear = (triControl&0x7f);
	else if (triLinear)
		triLinear--;
	if (!(triControl&0x80))
		triLinReload = 0;
}

/*
float createLowpass (final int order, final float fc, float fs) {
    float cutoff = fc / fs;
    float fir[order + 1];
    float factor = 2.0 * cutoff;
    int half = order >> 1;
    for(int i = 0; i < fir.length; i++)
    {
        fir[i] = factor * sinc(factor * (i - half));
    }
    return fir;
}

float sinc(float x)
{
    if(x != 0)
    {
        float xpi = Math.PI * x;
        return Math.sin(xpi) / xpi;
    }
    return 1.0;
}

float[] windowBlackman(float fir)
{
    int m = fir.length - 1;
    for(int i = 0; i < fir.length; i++)
    {
        fir[i] *= 0.42
            - 0.5 * Math.cos(2.0 * Math.PI * i / m)
            + 0.08 * Math.cos(4.0 * Math.PI * i / m);
    }
    return fir;
}
*/
