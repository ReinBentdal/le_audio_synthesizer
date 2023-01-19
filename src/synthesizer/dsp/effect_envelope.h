/**
 * @file effect_envelope.h
 * @author Rein Gundersen Bentdal
 * @date 2022-09-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _EFFECT_ENVELOPE_H
#define _EFFECT_ENVELOPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "integer_math.h"

/* the fade out magnitude where the audio is interpreted as silent */
#define FADE_OUT_THRESHOLD 1

enum envelope_state {
    ENVELOPE_STATE_SILENT,
    ENVELOPE_STATE_LOOP,
    ENVELOPE_STATE_HOLD,
    ENVELOPE_STATE_FADE_OUT,
};

enum envelope_mode {
    ENVELOPE_MODE_LOOP,
    ENVELOPE_MODE_ONE_SHOT,
    ENVELOPE_MODE_ONE_SHOT_HOLD,
};

struct effect_envelope {
    uint32_t phase_accumulator;
    int32_t phase_increment;

    float floor_level;
    float duty_cycle;
    float rising_curve;
    float falling_curve;
    float fade_out_attenuation;

    enum envelope_mode mode;
    enum envelope_state state;

    bool new_state;
    uint16_t magnitude_next;
};


/* standard interface */
void effect_envelope_init(struct effect_envelope* this);
bool effect_envelope_process(struct effect_envelope* this, fixed16* block, size_t block_size);

void effect_envelope_start(struct effect_envelope* this);
void effect_envelope_end(struct effect_envelope* this);

/* config */
void effect_envelope_set_period(struct effect_envelope* this, float ms);
void effect_envelope_set_duty_cycle(struct effect_envelope* this, float duty);
void effect_envelope_set_floor(struct effect_envelope* this, float floor);
void effect_envelope_set_rising_curve(struct effect_envelope* this, float curve);
void effect_envelope_set_falling_curve(struct effect_envelope* this, float curve);
void effect_envelope_set_mode(struct effect_envelope* this, enum envelope_mode mode);

void effect_envelope_set_fade_out_attenuation(struct effect_envelope* this, float attenuation);

bool effect_envelope_is_active(struct effect_envelope* this);

#endif