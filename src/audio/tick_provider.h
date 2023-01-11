/**
 * @file tick_provider.h
 * @author Rein Gundersen Bentdal
 * @brief Gives audio based time syncronization
 * @version 0.1
 * @date 2023-01-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _TICK_PROVIDER_H_
#define _TICK_PROVIDER_H_

#include <stdint.h>

/* number of ticks to send to subscribers for each quarter note increment */
#define PULSES_PER_QUARTER_NOTE 24

typedef void (*tick_provideer_notify_cb)(void);

void tick_provider_init(void);

void tick_provider_subscribe(tick_provideer_notify_cb);
void tick_provider_unsubscribe(tick_provideer_notify_cb);

void tick_provider_set_bpm(uint32_t bpm);

/** should be called for every processed audio block, for correct time syncronizaton */
void tick_provider_increment(void);

#endif