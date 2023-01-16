#ifndef _EFFECT_ECHO_H_
#define _EFFECT_ECHO_H_

#define EFFECT_ECHO_MAX_SAMPLES 44100

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct effect_echo {
    int16_t samples[EFFECT_ECHO_MAX_SAMPLES];
    uint32_t head_index;
    uint32_t tail_index;

    int16_t feedback_gain;
};

void effect_echo_init(struct effect_echo*);

bool effect_echo_process(struct effect_echo*, int8_t* block, size_t block_size);

void effect_echo_set_delay(struct effect_echo*, uint32_t delay_samples);

void effect_echo_set_feedback(struct effect_echo*, int16_t magnitute);

#endif