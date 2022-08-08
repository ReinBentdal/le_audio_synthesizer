#include "instrument.h"

#include <zephyr/kernel.h>
#include <arm_math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

static float _note_to_freq(int note);

void instrument_init(struct instrument* instrument) {
    __ASSERT(instrument != NULL, "instrument null pointer");

    for (int i = 0; i < CONFIG_MAX_NOTES; i++) {
        osc_init(&instrument->osciillators[i]);
        osc_set_amplitude(&instrument->osciillators[i], 0);

        effect_modulation_init(&instrument->modulation[i]);
        effect_modulation_set_amplitude(&instrument->modulation[i], 0.7f);
        effect_modulation_set_freq(&instrument->modulation[i], 2);
    }
}

void instrument_play_note(struct instrument* instrument, int index, int note) {
    __ASSERT(instrument != NULL, "instrument null pointer");
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    const float freq = _note_to_freq(note);
    osc_set_freq(&instrument->osciillators[index], freq);
    osc_set_amplitude(&instrument->osciillators[index], 1.0f / CONFIG_MAX_NOTES);
}

void instrument_stop_note(struct instrument* instrument, int index) {
    __ASSERT(instrument != NULL, "instrument null pointer");
    __ASSERT(index >= 0 && index < CONFIG_MAX_NOTES, "note index out of range");

    osc_set_amplitude(&instrument->osciillators[index], 0.0f);
}

bool instrument_process(struct instrument* instrument, int8_t* block, size_t block_size) {
	BUILD_ASSERT(CONFIG_AUDIO_BIT_DEPTH_OCTETS == 2, "instrument only support 16-bit");
	__ASSERT_NO_MSG(instrument != NULL);
	__ASSERT_NO_MSG(block != NULL);

    bool did_process = false;

    for (int i = 0; i < CONFIG_MAX_NOTES; i++) {
        if (osc_process_sine(&instrument->osciillators[i], block, block_size)) {
            effect_modulation_process(&instrument->modulation[i], block, block_size);
            did_process = true;
        }
    }
    return did_process;
}

static float _note_to_freq(int note) {
    return 440*powf(2, 1.f*(note - 69)/12);
}