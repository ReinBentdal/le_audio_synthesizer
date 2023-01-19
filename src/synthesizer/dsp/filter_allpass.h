/**
 * @file effect_envelope.h
 * @author Rein Gundersen Bentdal
 * @date 2023-01-17
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _FILTER_ALLPASS_H_
#define _FILTER_ALLPASS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "integer_math.h"

struct filter_allpass {
    fixed16* buffer;
    size_t buffer_size;
    uint32_t head_index;
    uint32_t tail_index;

    fixed16 gain;
    fixed16 gain2;
};

void filter_allpass_init(struct filter_allpass*, fixed16* buffer, size_t buffer_size);

void filter_allpass_set_gain(struct filter_allpass*, fixed16 gain);

void filter_allpass_set_delay(struct filter_allpass*, uint32_t delay_ms);

bool filter_allpass_process(struct filter_allpass*, fixed16* block, size_t block_size);

#endif
