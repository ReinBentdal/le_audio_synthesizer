/**
 * @file tick_provider.h
 * @author Rein Gundersen Bentdal
 * @brief Gives audio based time syncronization. Time is incremented by sending ticks to subscribers.
 * @version 0.1
 * @date 2023-01-11
 * 
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TICK_PROVIDER_H_
#define _TICK_PROVIDER_H_

#include <stdint.h>

/* number of ticks to send to subscribers for each quarter note increment */
#define PULSES_PER_QUARTER_NOTE 24


typedef void (*tick_provider_notify_cb)(void);

struct tick_provider_subscriber
{
    tick_provider_notify_cb notifier;
    struct tick_provider_subscriber *next;
};


void tick_provider_init(void);

void tick_provider_subscribe(struct tick_provider_subscriber*, tick_provider_notify_cb);
void tick_provider_unsubscribe(struct tick_provider_subscriber*);

void tick_provider_set_bpm(uint32_t bpm);

/** should be called for every processed audio block, for correct time syncronizaton */
void tick_provider_increment(void);

#endif