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

void tick_provider_increment(void);

#endif