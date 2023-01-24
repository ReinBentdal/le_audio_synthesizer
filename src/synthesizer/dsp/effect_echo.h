/**
 * @file effect_echo.h
 * @author Rein Gundersen Bentdal
 * @brief Audio echo/delay effect
 * @date 2023-01-24
 * 
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _EFFECT_ECHO_H_
#define _EFFECT_ECHO_H_

#include <stddef.h>
#include <stdbool.h>

#include "integer_math.h"

struct effect_echo {
    fixed16* buffer;
    size_t buffer_size;
    uint32_t head_index;
    uint32_t tail_index;

    fixed16 feedback_gain;
};

void effect_echo_init(struct effect_echo*, fixed16* buffer, size_t buffer_size);

bool effect_echo_process(struct effect_echo*, fixed16* block, size_t block_size);

void effect_echo_set_delay(struct effect_echo*, uint32_t delay_ms);

void effect_echo_set_feedback(struct effect_echo*, fixed16 magnitute);

#endif