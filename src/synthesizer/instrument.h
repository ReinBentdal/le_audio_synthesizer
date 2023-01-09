/**
 * @file effect_envelope.h
 * @author Rein Gundersen Bentdal
 * @date 2022-09-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _AUDIO_INSTRUMENT_H_
#define _AUDIO_INSTRUMENT_H_

#include <stdbool.h>

#include "dsp/oscillator.h"
#include "dsp/effect_modulation.h"
#include "dsp/effect_envelope.h"

struct instrument {
    struct oscillator osciillators[CONFIG_MAX_NOTES];
    struct effect_modulation modulation[CONFIG_MAX_NOTES];
    struct effect_envelope envelopes[CONFIG_MAX_NOTES];
};

void instrument_init(struct instrument* instrument);

void instrument_play_note(struct instrument* instrument, int index, int note);
void instrument_stop_note(struct instrument* instrument, int index);

/* returns false if nothing was processed */
bool instrument_process(struct instrument* instrument, int8_t* block, size_t block_size);

#endif