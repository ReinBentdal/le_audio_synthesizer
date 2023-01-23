#include "synthesizer.h"

#include <zephyr/kernel.h>

#include "dsp_instructions.h"
#include "midi_note_to_frequency.h"
#include "integer_math.h"

#include "arpeggio.h"
#include "dsp/oscillator.h"
#include "dsp/effect_modulation.h"
#include "dsp/effect_envelope.h"
#include "dsp/effect_echo.h"
#include "dsp/filter_allpass.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

#define NOTE_BASE 40+12
static const uint8_t key_map[] = {NOTE_BASE-3-12, NOTE_BASE+2, NOTE_BASE+6, NOTE_BASE+9, NOTE_BASE+14};

static struct oscillator _osciillators[CONFIG_MAX_NOTES];
static struct effect_modulation _modulation[CONFIG_MAX_NOTES];
static struct effect_envelope _envelopes[CONFIG_MAX_NOTES];

#define _ECHO_BUF_SIZE 24000
static fixed16 _echo_buf[_ECHO_BUF_SIZE];
static struct effect_echo _echo;


static void _play_note(int index, int note);
static void _stop_note(int index);
static inline void _audio_stream_add(fixed16* destination, fixed16* source, size_t block_size);

void synthesizer_init()
{
    arpeggio_init(_play_note, _stop_note);
    arpeggio_set_divider(12);

    effect_echo_init(&_echo, _echo_buf, ARRAY_SIZE(_echo_buf));
    effect_echo_set_delay(&_echo, 500);
    effect_echo_set_feedback(&_echo, FLOAT_TO_FIXED16(0.4));

    /* configure parameters of the synthesizer */
    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        osc_init(&_osciillators[i]);
        osc_set_amplitude(&_osciillators[i], FLOAT_TO_FIXED16(0));

        effect_modulation_init(&_modulation[i]);
        effect_modulation_set_amplitude(&_modulation[i], 0.7f);
        effect_modulation_set_freq(&_modulation[i], 2);

        effect_envelope_init(&_envelopes[i]);
        effect_envelope_set_period(&_envelopes[i], 150);
        effect_envelope_set_duty_cycle(&_envelopes[i], 0.1f);
        effect_envelope_set_mode(&_envelopes[i], ENVELOPE_MODE_ONE_SHOT);
        effect_envelope_set_floor(&_envelopes[i], 0.2f);
        effect_envelope_set_fade_out_attenuation(&_envelopes[i], 0.04f);
    }
}

void synthesizer_key_event(struct button_event* button_event) {
    __ASSERT_NO_MSG(button_event != NULL);
    
    switch (button_event->state) {
        case BUTTON_PRESSED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            const uint8_t note = key_map[button_event->index];
            arpeggio_note_add(note);

            LOG_DBG("button %u pressed", button_event->index);

            break;
        }
        case BUTTON_RELEASED: {
            __ASSERT(button_event->index <= ARRAY_SIZE(key_map), "button index out of range");

            const uint8_t note = key_map[button_event->index];
            arpeggio_note_remove(note);

            LOG_DBG("button %u released", button_event->index);

            break;
        }
    }
}

bool synthesizer_process(fixed16* block, size_t block_size)
{
    BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "synthesizer only support 16-bit");
    __ASSERT_NO_MSG(block != NULL);

    for (int i = 0; i < CONFIG_MAX_NOTES; i++)
    {
        bool ret;

        /* if the oscillator envelope is 0, there will be no output audio */
        ret = effect_envelope_is_active(&_envelopes[i]);
        if (ret == false) continue;

        fixed16 osc_block[block_size];
        
        ret = osc_process_triangle(&_osciillators[i], osc_block, block_size);
        if (ret == false) continue;

        // ret = effect_modulation_process(&_modulation[i], block, block_size);
        // if (ret == false) continue;

        /* oscillator envelope, similar to ADSR */
        ret = effect_envelope_process(&_envelopes[i], osc_block, block_size);
        if (ret == false) continue;

        /* add oscillator to audio stream */
        _audio_stream_add(block, osc_block, block_size);
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
    osc_set_amplitude(&_osciillators[index], FLOAT_TO_FIXED16(1.0f / CONFIG_MAX_NOTES));

    effect_envelope_start(&_envelopes[index]);
}

static void _stop_note(int index)
{
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    effect_envelope_end(&_envelopes[index]);
}

static inline void _audio_stream_add(fixed16* destination, fixed16* source, size_t block_size) {

    uint32_t *dst = (uint32_t *)destination;
    const uint32_t *src = (uint32_t *)source;
    const uint32_t *end = (uint32_t *)(destination + block_size);

    do {
        /* adds 4 samples for each loop cycle */
        uint32_t tmp = *dst;
        *dst++ = signed_add_16_and_16(tmp, *src++);
        tmp = *dst;
        *dst++ = signed_add_16_and_16(tmp, *src++);
    } while (dst < end);
}