#include "effect_modulation.h"

#include <zephyr/sys/__assert.h>
#include <zephyr/kernel.h>

#include "waveforms.h"

void effect_modulation_init(struct effect_modulation* mod)
{
	__ASSERT_NO_MSG(mod != NULL);

	*mod = (struct effect_modulation){
			.magnitude = FLOAT_TO_UFIXED16(1.0),
			.phase_accumulate = 0,
			.phase_increment = 0,
	};
}

void effect_modulation_set_amplitude(struct effect_modulation *mod, ufixed16 magnitude)
{
	__ASSERT_NO_MSG(mod != NULL);

	mod->magnitude = magnitude;
}

void effect_modulation_set_freq(struct effect_modulation *mod, float freq)
{
	__ASSERT_NO_MSG(mod != NULL);
	__ASSERT(freq >= 0 && freq < CONFIG_AUDIO_SAMPLE_RATE_HZ / 2, "effect modulation frequency out of range");

	mod->phase_increment = (freq / CONFIG_AUDIO_SAMPLE_RATE_HZ / 2) * UINT32_MAX;
}

bool effect_modulation_process(struct effect_modulation *mod, fixed16* block, size_t block_size)
{
	BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "modulation only support 16-bit");
	__ASSERT_NO_MSG(mod != NULL);
	__ASSERT_NO_MSG(mod != NULL);

	if (mod->magnitude == 0) {
		return true;
	}

	for (uint32_t i = 0; i < block_size; i++)
	{
		/* upper 8 bit as 256-value sample index */
		uint32_t wave_index = mod->phase_accumulate >> 24;

		/* interpolate between the two samples for better audio quality */
		const uint16_t interpolate_pos = (mod->phase_accumulate >> 8) & UINT16_MAX; // 16 bit scale value
		const fixed16 modulation_sample = FIXED_INTERPOLATE_AND_SCALE(sinus_samples[wave_index], sinus_samples[wave_index+1], interpolate_pos, mod->magnitude);

        /* multiply modulation with waveform */
		block[i] = FIXED_MULTIPLY(block[i], modulation_sample);

		/* increment waveform phase */
		mod->phase_accumulate += mod->phase_increment;
	}

	return true;
}