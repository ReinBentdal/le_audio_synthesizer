/*
 *  Copyright (c) 2021, PACKETCRAFT, INC.
 *
 *  SPDX-License-Identifier: LicenseRef-PCFT
 */

#ifndef _AUDIO_SYNC_TIMER_H_
#define _AUDIO_SYNC_TIMER_H_

#include <stdint.h>

/**
 * @brief Get the currrent timer value
 *
 * @return Captured value
 */
uint32_t audio_sync_timer_curr_time_get(void);

/**
 * @brief Send audio timer sync event
 *
 * @note This event clears the sync timer on the
 * nRF5340 APP core and NET core at exactly the
 * same time, making the two timers synchronized
 */
void audio_sync_timer_sync_evt_send(void);

/**
 * @brief Initialize the audio sync timer module
 *
 * @return 0 if successful, error otherwise
 */
int audio_sync_timer_init(void);

#endif /* _AUDIO_SYNC_TIMER_H_ */
