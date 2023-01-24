/**
 * @file effect_envelope.h
 * @author Rein Gundersen Bentdal
 * @date 2022-09-01
 * 
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _AUDIO_INSTRUMENT_H_
#define _AUDIO_INSTRUMENT_H_

#include <stdbool.h>

#include "../io/button.h"
#include "integer_math.h"

void synthesizer_init(void);

void synthesizer_key_event(struct button_event*);

/* returns false if nothing was processed */
bool synthesizer_process(fixed16* block, size_t block_size);

/* compatible with type tick_provider_notify_cb */
void synthesizer_tick(void);

#endif