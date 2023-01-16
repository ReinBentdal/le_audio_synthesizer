#include "synthesizer.h"

#include <zephyr/kernel.h>
#include <arm_math.h>

#include "dsp_instructions.h"
#include "midi_note_to_frequency.h"

#include "arpeggio.h"
#include "dsp/oscillator.h"
#include "dsp/effect_modulation.h"
#include "dsp/effect_envelope.h"
#include "dsp/effect_echo.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

#define NOTE_BASE 40
static const uint8_t key_map[] = {NOTE_BASE-3-12, NOTE_BASE+2, NOTE_BASE+6, NOTE_BASE+9, NOTE_BASE+14};

static struct oscillator _osciillators[CONFIG_MAX_NOTES];
static struct effect_modulation _modulation[CONFIG_MAX_NOTES];
static struct effect_envelope _envelopes[CONFIG_MAX_NOTES];
static struct effect_echo _echo;

static void _play_note(int index, int note);
static void _stop_note(int index);
static inline void _audio_stream_add(int8_t* destination, int8_t* source, size_t block_size);

void synthesizer_init()
{
    arpeggio_init(_play_note, _stop_note);
    arpeggio_set_divider(12);

    effect_echo_init(&_echo);
    effect_echo_set_delay(&_echo, EFFECT_ECHO_MAX_SAMPLES);
    effect_echo_set_feedback(&_echo, INT16_MAX / 2);

    /* configure parameters of the synthesizer */
    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        osc_init(&_osciillators[i]);
        osc_set_amplitude(&_osciillators[i], 0);

        effect_modulation_init(&_modulation[i]);
        effect_modulation_set_amplitude(&_modulation[i], 0.7f);
        effect_modulation_set_freq(&_modulation[i], 2);

        effect_envelope_init(&_envelopes[i]);
        effect_envelope_set_periode(&_envelopes[i], 150);
        effect_envelope_set_duty_cycle(&_envelopes[i], 0.9f);
        effect_envelope_set_mode(&_envelopes[i], ENVELOPE_MODE_ONE_SHOT);
        effect_envelope_set_floor(&_envelopes[i], 0.2f);
        effect_envelope_set_fade_out_attenuation(&_envelopes[i], 0.02f);
    }
}

void synthesizer_key_event(struct button_event* button_event) {
    __ASSERT_NO_MSG(button_event != NULL);
    
    switch (button_event->state) {
        case BUTTON_PRESSED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            const uint8_t note = key_map[button_event->index];
            arpeggio_note_add(note);

            break;
        }
        case BUTTON_RELEASED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            const uint8_t note = key_map[button_event->index];
            arpeggio_note_remove(note);

            break;
        }
    }
}

bool synthesizer_process(int8_t *block, size_t block_size)
{
    BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "synthesizer only support 16-bit");
    __ASSERT_NO_MSG(block != NULL);

    bool output_is_zero = true;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        bool ret;

        /* if the oscillator envelope is 0, there will be no output audio */
        ret = effect_envelope_is_active(&_envelopes[i]);
        if (ret == false) continue;

        int8_t osc_block[CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size];
        memset(osc_block, 0, CONFIG_AUDIO_BIT_DEPTH_OCTETS*block_size * sizeof osc_block[0]);
        
        ret = osc_process_sawtooth(&_osciillators[i], osc_block, block_size);
        if (ret == false) continue;

        // ret = effect_modulation_process(&_modulation[i], block, block_size);
        // if (ret == false) continue;

        /* oscillator envelope, similar to ADSR */
        ret = effect_envelope_process(&_envelopes[i], osc_block, block_size);
        if (ret == false) continue;

        /* add oscillator to audio stream */
        _audio_stream_add(block, osc_block, block_size);

        output_is_zero = false;
    }

    /* echo effect effecting all oscillators */
    (void)effect_echo_process(&_echo, block, block_size);
    
    return true;
}

void synthesizer_tick(void) {
    arpeggio_tick();
}

static void _play_note(int index, int note)
{
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    const float freq = midi_note_to_frequency[note];
    osc_set_freq(&_osciillators[index], freq);
    osc_set_amplitude(&_osciillators[index], 1.0f / CONFIG_MAX_NOTES);

    effect_envelope_start(&_envelopes[index]);
}

static void _stop_note(int index)
{
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    effect_envelope_end(&_envelopes[index]);
}

static inline void _audio_stream_add(int8_t* destination, int8_t* source, size_t block_size) {
        #if (CONFIG_EXPLICIT_DSP_INSTRUCTIONS)
        uint32_t *dst = (uint32_t *)destination;
        const uint32_t *src = (uint32_t *)source;
        const uint32_t *end = (uint32_t *)(((int16_t *)destination) + block_size);

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
            ((int16_t *)destination)[i] += ((int16_t *)source)[i];
        }
        #endif
}