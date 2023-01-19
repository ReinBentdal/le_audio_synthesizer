#include "oscillator.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include <math.h>

#include "waveforms.h"
#include "dsp_instructions.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dsp, CONFIG_LOG_DSP_LEVEL);

void osc_init(struct oscillator* osc)
{
  __ASSERT_NO_MSG(osc != NULL);

  *osc = (struct oscillator) {
    .magnitude = FLOAT_TO_FIXED16(1.0),
    .phase_accumulate = 0,
    .phase_increment = 0,
  };
}

void osc_set_amplitude(struct oscillator* osc, fixed16 magnitude)
{
  __ASSERT_NO_MSG(osc != NULL);

  osc->magnitude = magnitude;
}

void osc_set_freq(struct oscillator* osc, float freq)
{
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT(freq >= 0 && freq < CONFIG_AUDIO_SAMPLE_RATE_HZ / 2, "oscillator frequency block of range");

  osc->phase_increment = (freq / CONFIG_AUDIO_SAMPLE_RATE_HZ / 2) * UINT32_MAX;
}

bool osc_process_sine(struct oscillator* osc, fixed16* block, size_t block_size)
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

    /* interpolate between the two samples for better audio quality */
    uint32_t interpolate_pos = (osc->phase_accumulate >> 8) & UINT16_MAX;

    block[i] = FIXED_INTERPOLATE_AND_SCALE(sinus_samples[wave_index], sinus_samples[wave_index + 1], interpolate_pos, osc->magnitude);

    /* increment waveform phase */
    osc->phase_accumulate += osc->phase_increment;
  }

  return true;
}

bool osc_process_sinecrush(struct oscillator* osc, fixed16* block, size_t block_size)
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

    /* interpolate between the two samples for better audio quality */
    uint32_t interpolate_pos = (osc->phase_accumulate >> 8) & UINT16_MAX;

    block[i] = FIXED_INTERPOLATE_AND_SCALE(sinus_samples[wave_index + 1], sinus_samples[wave_index], interpolate_pos, osc->magnitude);

    /* increment waveform phase */
    osc->phase_accumulate += osc->phase_increment;
  }

  return true;
}

bool osc_process_triangle(struct oscillator* osc, fixed16* block, size_t block_size)
{
  BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "oscillator only support 16-bit");
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT_NO_MSG(block != NULL);

  if (osc->magnitude == 0) {
    return false;
  }

  for (int i = 0; i < block_size; i++) {
    uint32_t phtop = osc->phase_accumulate >> 30;
    if (phtop == 1 || phtop == 2) {
      block[i] = UFIXED_MULTIPLY(0xFFFF - ((int32_t)osc->phase_accumulate >> 15), osc->magnitude);
    } else {
      block[i] = UFIXED_MULTIPLY((int32_t)osc->phase_accumulate >> 15, osc->magnitude);
    }
    osc->phase_accumulate += osc->phase_increment;
  }

	return true;
}

bool osc_process_sawtooth(struct oscillator* osc, fixed16* block, size_t block_size) {
  BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "oscillator only support 16-bit");
  __ASSERT_NO_MSG(osc != NULL);
  __ASSERT_NO_MSG(block != NULL);

  if (osc->magnitude == 0) {
    return false;
  }

  for (int i = 0; i < block_size; i++) {
    block[i] = signed_multiply_32x16t(osc->magnitude, osc->phase_accumulate);

    osc->phase_accumulate += osc->phase_increment;
  }

  return true;
}