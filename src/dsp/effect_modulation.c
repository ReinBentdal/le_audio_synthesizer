#include "effect_modulation.h"

#include <zephyr/sys/__assert.h>
#include <zephyr/kernel.h>

#include "waveforms.h"


void effect_modulation_init(struct effect_modulation* mod)
{
	__ASSERT_NO_MSG(mod != NULL);

	*mod = (struct effect_modulation){
			.magnitude = UINT16_MAX,
			.phase_accumulate = 0,
			.phase_increment = 0,
	};
}

bool effect_modulation_process(struct effect_modulation *mod, int8_t* block, size_t block_size)
{
	BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "modulation only support 16-bit");
	__ASSERT_NO_MSG(mod != NULL);
	__ASSERT_NO_MSG(mod != NULL);

	if (mod->magnitude == 0) {
		return false;
	}

	for (uint32_t i = 0; i < block_size; i++)
	{
		/* upper 8 bit as 256-value sample index */
		uint32_t wave_index = mod->phase_accumulate >> 24;

		int32_t lower_sample = sinus_samples[wave_index];
		int32_t upper_sample = sinus_samples[wave_index + 1];

		/* interpolate between the two samples for better audio quality */
		uint32_t scale = (mod->phase_accumulate >> 8) & 0x0000FFFF;
		upper_sample *= scale;
		lower_sample *= 0x10000 - scale;

		int32_t modulation_magnitude = (((lower_sample + upper_sample) >> 16) * mod->magnitude) >> 16;

        /* multiply modulation with waveform */
        ((int16_t*)block)[i] = (((int16_t*)block)[i]*modulation_magnitude) >> 15;

		/* increment waveform phase */
		mod->phase_accumulate += mod->phase_increment;
	}

	return true;
}

void effect_modulation_set_amplitude(struct effect_modulation *mod, float amplitude)
{
	__ASSERT_NO_MSG(mod != NULL);
	__ASSERT(amplitude >= 0 && amplitude <= 1, "effect modulation amplitude out of range");

	mod->magnitude = UINT16_MAX * amplitude;
}

void effect_modulation_set_freq(struct effect_modulation *mod, float freq)
{
	__ASSERT_NO_MSG(mod != NULL);
	__ASSERT(freq >= 0 && freq < CONFIG_AUDIO_SAMPLE_RATE_HZ / 2, "effect modulation frequency out of range");

	mod->phase_increment = (freq / CONFIG_AUDIO_SAMPLE_RATE_HZ / 2) * UINT32_MAX;
}