#include "instrument.h"

#include <zephyr/kernel.h>
#include <arm_math.h>

#include "dsp_instructions.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

static inline float _note_to_freq(int note);

void instrument_init(struct instrument *this)
{
    __ASSERT(this != NULL, "instrument null pointer");

    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        osc_init(&this->osciillators[i]);
        osc_set_amplitude(&this->osciillators[i], 0);

        effect_modulation_init(&this->modulation[i]);
        effect_modulation_set_amplitude(&this->modulation[i], 0.7f);
        effect_modulation_set_freq(&this->modulation[i], 2);

        effect_envelope_init(&this->envelopes[i]);
        effect_envelope_set_periode(&this->envelopes[i], 1000);
    }
}

void instrument_play_note(struct instrument *this, int index, int note)
{
    __ASSERT(this != NULL, "instrument null pointer");
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    const float freq = _note_to_freq(note);
    osc_set_freq(&this->osciillators[index], freq);
    osc_set_amplitude(&this->osciillators[index], 1.0f / CONFIG_MAX_NOTES);

    effect_envelope_start(&this->envelopes[index]);
}

void instrument_stop_note(struct instrument *this, int index)
{
    __ASSERT(this != NULL, "instrument null pointer");
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    effect_envelope_end(&this->envelopes[index]);
}

bool instrument_process(struct instrument *this, int8_t *block, size_t block_size)
{
    BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "instrument only support 16-bit");
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT_NO_MSG(block != NULL);

    bool did_process = false;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        if (effect_envelope_is_active(&this->envelopes[i]))
        {

            /* each oscillators directly modifies osc_block by appending its waveform. Thus a separate stage to combine each oscillator waveform is not necessary */

            int8_t osc_block[CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size];

            memset(osc_block, 0, CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size * sizeof osc_block[0]);
            
            if (osc_process_sawtooth(&this->osciillators[i], osc_block, block_size))
            {
                // if(effect_modulation_process(&this->modulation[i], block, block_size)) {
                // TODO: cannot assume effect_envelope_is_active returns the same in this position. Should force DSP to run without interrupts from other threads
                if (effect_envelope_process(&this->envelopes[i], osc_block, block_size))
                {

                    /* add oscillator to audio stream */
#if (CONFIG_EXPLICIT_DSP_INSTRUCTIONS)
                    uint32_t *dst = (uint32_t *)block;
                    const uint32_t *src = (uint32_t *)osc_block;
                    const uint32_t *end = (uint32_t *)(((int16_t *)block) + block_size);

                    do
                    {
                        uint32_t tmp = *dst;
                        *dst++ = signed_add_16_and_16(tmp, *src++);
                        tmp = *dst;
                        *dst++ = signed_add_16_and_16(tmp, *src++);
                    } while (dst < end);
#else
                    for (int i = 0; i < block_size; i++)
                    {
                        ((int16_t *)block)[i] += ((int16_t *)osc_block)[i];
                    }
#endif

                    did_process = true;
                }
                // }

                did_process = true;
            }
        }
    }
    return did_process;
}

static inline float _note_to_freq(int note)
{
    return 440 * powf(2, 1.f * (note - 69) / 12);
}
