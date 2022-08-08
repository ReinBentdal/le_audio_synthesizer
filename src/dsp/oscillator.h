#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLK_PERIOD_US 1000

/* Single audio block size in number of samples (stereo) */
#define BLK_SIZE_SAMPLES(r) (((r)*BLK_PERIOD_US) / 1000000)

#define BLK_MONO_NUM_SAMPS BLK_SIZE_SAMPLES(CONFIG_AUDIO_SAMPLE_RATE_HZ)
#define BLK_MONO_SIZE_OCTETS (BLK_MONO_NUM_SAMPS * CONFIG_AUDIO_BIT_DEPTH_OCTETS)

struct oscillator
{
	int16_t magnitude;
	uint32_t phase_increment;
	uint32_t phase_accumulate;
};

/* standard interface */
void osc_init(struct oscillator* osc);
bool osc_process_sine(struct oscillator* osc, int8_t* block, size_t block_size);
bool osc_process_triangle(struct oscillator* osc, int8_t* block, size_t block_size);

/* config */
void osc_set_amplitude(struct oscillator* osc, float amplitude);
void osc_set_freq(struct oscillator* osc, float freq);