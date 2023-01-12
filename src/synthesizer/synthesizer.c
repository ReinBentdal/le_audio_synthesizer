#include "synthesizer.h"

#include <zephyr/kernel.h>
#include <arm_math.h>

#include "dsp_instructions.h"

#include "arpeggio.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

#define NOTE_BASE 40
static const uint8_t key_map[] = {NOTE_BASE-3, NOTE_BASE+2, NOTE_BASE+6, NOTE_BASE+9, NOTE_BASE+10};

static struct oscillator osciillators[CONFIG_MAX_NOTES];
static struct effect_modulation modulation[CONFIG_MAX_NOTES];
static struct effect_envelope envelopes[CONFIG_MAX_NOTES];

static inline float _note_to_freq(int note);
static void _play_note(int index, int note);
static void _stop_note(int index);

void synthesizer_init()
{
    arpeggio_init(_play_note, _stop_note);

    /* configure parameters of the synthesizer */
    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        osc_init(&osciillators[i]);
        osc_set_amplitude(&osciillators[i], 0);

        effect_modulation_init(&modulation[i]);
        effect_modulation_set_amplitude(&modulation[i], 0.7f);
        effect_modulation_set_freq(&modulation[i], 2);

        effect_envelope_init(&envelopes[i]);
        effect_envelope_set_periode(&envelopes[i], 100);
        effect_envelope_set_duty_cycle(&envelopes[i], 0.0f);
        effect_envelope_set_mode(&envelopes[i], ENVELOPE_MODE_ONE_SHOT);
        effect_envelope_set_floor(&envelopes[i], 0.1f);
        effect_envelope_set_fade_out_attenuation(&envelopes[i], 0.02f);
    }
}

void synthesizer_key_event(struct button_event* button_event) {
    __ASSERT_NO_MSG(button_event != NULL);
    
    switch (button_event->state) {
        case BUTTON_PRESSED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            arpeggio_note_add(key_map[button_event->index]);

            break;
        }
        case BUTTON_RELEASED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            arpeggio_note_remove(key_map[button_event->index]);

            break;
        }
    }
}



bool synthesizer_process(int8_t *block, size_t block_size)
{
    BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "synthesizer only support 16-bit");
    __ASSERT_NO_MSG(block != NULL);

    bool did_process = false;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        if (effect_envelope_is_active(&envelopes[i]))
        {

            /* each oscillators directly modifies osc_block by appending its waveform. Thus a separate stage to combine each oscillator waveform is not necessary */

            int8_t osc_block[CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size];

            memset(osc_block, 0, CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size * sizeof osc_block[0]);
            
            if (osc_process_triangle(&osciillators[i], osc_block, block_size))
            {
                // if(effect_modulation_process(&modulation[i], block, block_size)) {
                // TODO: cannot assume effect_envelope_is_active returns the same in this position. Should force DSP to run without interrupts from other threads
                if (effect_envelope_process(&envelopes[i], osc_block, block_size))
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

void synthesizer_tick(void) {
    arpeggio_tick();
}

static inline float _note_to_freq(int note)
{
    return 440 * powf(2, 1.f * (note - 69) / 12);
}

static void _play_note(int index, int note)
{
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    const float freq = _note_to_freq(note);
    osc_set_freq(&osciillators[index], freq);
    osc_set_amplitude(&osciillators[index], 1.0f / CONFIG_MAX_NOTES);

    effect_envelope_start(&envelopes[index]);
}

static void _stop_note(int index)
{
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    effect_envelope_end(&envelopes[index]);
}