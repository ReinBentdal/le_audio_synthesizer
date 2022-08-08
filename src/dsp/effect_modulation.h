#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLK_PERIOD_US 1000

/* Single audio block size in number of samples (stereo) */
#define BLK_SIZE_SAMPLES(r) (((r)*BLK_PERIOD_US) / 1000000)

#define BLK_MONO_NUM_SAMPS BLK_SIZE_SAMPLES(CONFIG_AUDIO_SAMPLE_RATE_HZ)
#define BLK_MONO_SIZE_OCTETS (BLK_MONO_NUM_SAMPS * CONFIG_AUDIO_BIT_DEPTH_OCTETS)

struct effect_modulation
{
	uint16_t magnitude;
	uint32_t phase_increment;
	uint32_t phase_accumulate;
};

/* standard interface */
void effect_modulation_init(struct effect_modulation* mod);
bool effect_modulation_process(struct effect_modulation* mod, int8_t* block, size_t block_size);

/* config */
void effect_modulation_set_amplitude(struct effect_modulation* mod, float amplitude);
void effect_modulation_set_freq(struct effect_modulation* mod, float freq);