#include "oscillator.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>

#include "waveforms.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dsp, CONFIG_LOG_DSP_LEVEL);

void osc_init(struct oscillator* osc)
{
  __ASSERT_NO_MSG(osc != NULL);

  *osc = (struct oscillator) {
    .magnitude = INT16_MAX,
    .phase_accumulate = 0,
    .phase_increment = 0,
  };
}

bool osc_process_sine(struct oscillator* osc, int8_t* block, size_t block_size)
{
  BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "oscillator only support 16-bit");
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT_NO_MSG(block != NULL);

  if (osc->magnitude == 0) {
    return false;
  }

  for (uint32_t i = 0; i < block_size; i++) {
    /* upper 8 bit as 256-value sample index */
    uint32_t wave_index = osc->phase_accumulate >> 24;

    int32_t lower_sample = sinus_samples[wave_index];
    int32_t upper_sample = sinus_samples[wave_index + 1];

    /* interpolate between the two samples for better audio quality */
    uint32_t scale = (osc->phase_accumulate >> 8) & 0x0000FFFF;
    upper_sample *= scale;
    lower_sample *= 0x10000 - scale;

    /* integer mean and 15-bit shift scale to 16-bit integer. Integer magnitude multiply */
    ((int16_t*)block)[i] += (((lower_sample + upper_sample) >> 16) * osc->magnitude) >> 16;

    /* increment waveform phase */
    osc->phase_accumulate += osc->phase_increment;
  }

  return true;
}

bool osc_process_triangle(struct oscillator* osc, int8_t* block, size_t block_size)
{
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT_NO_MSG(block != NULL);

  if (osc->magnitude == 0) {
    return false;
  }

  for (int i = 0; i < block_size; i++) {
    uint32_t phtop = osc->phase_accumulate >> 30;
    if (phtop == 1 || phtop == 2) {
      ((int16_t*)block)[i] += ((0xFFFF - (osc->phase_accumulate >> 15)) * osc->magnitude) >> 16;
    } else {
      ((int16_t*)block)[i] += (((int32_t)osc->phase_accumulate >> 15) * osc->magnitude) >> 16;
    }
    osc->phase_accumulate += osc->phase_increment;
  }

	return true;
}

void osc_set_amplitude(struct oscillator* osc, float amplitude)
{
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT(amplitude >= 0 && amplitude <= 1, "oscillator amplitude block of range");

  osc->magnitude = INT16_MAX * amplitude;
}

void osc_set_freq(struct oscillator* osc, float freq)
{
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT(freq >= 0 && freq < CONFIG_AUDIO_SAMPLE_RATE_HZ / 2, "oscillator frequency block of range");

  osc->phase_increment = (freq / CONFIG_AUDIO_SAMPLE_RATE_HZ / 2) * UINT32_MAX;
}