#ifndef _AUDIO_INSTRUMENT_H_
#define _AUDIO_INSTRUMENT_H_

#include "oscillator.h"
#include "effect_modulation.h"
#include <stdbool.h>

struct instrument {
    struct oscillator osciillators[CONFIG_MAX_NOTES];
    struct effect_modulation modulation[CONFIG_MAX_NOTES];
};

void instrument_init(struct instrument* instrument);

void instrument_play_note(struct instrument* instrument, int index, int note);
void instrument_stop_note(struct instrument* instrument, int index);

/* returns false if nothing was processed */
bool instrument_process(struct instrument* instrument, int8_t* block, size_t block_size);

#endif