/**
 * @file effect_envelope.h
 * @author Rein Gundersen Bentdal
 * @date 2022-09-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "integer_math.h"

#define BLK_PERIOD_US 1000

/* Single audio block size in number of samples (stereo) */
#define BLK_SIZE_SAMPLES(r) (((r)*BLK_PERIOD_US) / 1000000)

#define BLK_MONO_NUM_SAMPS BLK_SIZE_SAMPLES(CONFIG_AUDIO_SAMPLE_RATE_HZ)
#define BLK_MONO_SIZE_OCTETS (BLK_MONO_NUM_SAMPS * CONFIG_AUDIO_BIT_DEPTH_OCTETS)

struct effect_modulation
{
	ufixed16 magnitude;
	uint32_t phase_increment;
	uint32_t phase_accumulate;
};

/* standard interface */
void effect_modulation_init(struct effect_modulation* mod);
bool effect_modulation_process(struct effect_modulation* mod, fixed16* block, size_t block_size);

/* config */
void effect_modulation_set_amplitude(struct effect_modulation* mod, ufixed16 magnitude);
void effect_modulation_set_freq(struct effect_modulation* mod, float freq);