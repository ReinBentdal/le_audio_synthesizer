/**
 * @file key_assign.h
 * @author Rein Gundersen Bentdal
 * @brief Keeps track of which keys are active and to which instrument oscillator bank to assign note
 * @version 0.1
 * @date 2023-01-11
 * 
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _KEY_ASSIGN_H_
#define _KEY_ASSIGN_H_

#include <stdint.h>
#include <zephyr/kernel.h>

typedef void(*key_play_cb)(int index, int note);
typedef void(*key_stop_cb)(int index);

struct key {
    uint8_t index;
    uint8_t note;
    // velocity

    struct key* next;
};

struct keys {
    struct key _keys[CONFIG_MAX_NOTES];
    struct key* head;
    const key_play_cb play_cb;
    const key_stop_cb stop_cb;

    struct k_mutex mutex;
};


void keys_init(struct keys* keys, key_play_cb play_cb, key_stop_cb stop_cb);

void keys_play(struct keys* keys, int note);
void keys_stop(struct keys* keys, int note);

void keys_print(struct keys* keys);

#endif
