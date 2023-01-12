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
#include "../io/button.h"


void synthesizer_init(void);

void synthesizer_key_event(struct button_event*);

/* returns false if nothing was processed */
bool synthesizer_process(int8_t* block, size_t block_size);

/* compatible with type tick_provider_notify_cb */
void synthesizer_tick(void);

#endif