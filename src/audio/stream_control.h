#ifndef _STREAM_CONTROL_H_
#define _STREAM_CONTROL_H_

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stddef.h>
#include "ble_transmit.h"

/* State machine states for peer/stream */
enum stream_state {
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_LINK_READY,
	STATE_STREAMING,
	STATE_PAUSED,
	STATE_DISCONNECTED,
};

/** @brief Start streamctrl
 *
 *  @return 0 if successful.
 */
int stream_control_start(void);

/** @brief Drives streamctrl state machine
 *
 * This function drives the streamctrl state machine.
 * The function will read and handle events coming in to the streamctrl
 * module, and configure/run the rest of the system accordingly.
 *
 * Run this function repeatedly, e.g. in an eternal loop, to keep the
 * system running.
 */
void stream_control_event_handler(void);

/** @brief Get current streaming state
 *
 * @return      stream_state enum value
 */
enum stream_state stream_control_state_get(void);

/** @brief Send encoded data over the stream
 *
 * @param data	Data to send
 * @param len	Length of data to send
 */
void stream_control_encoded_data_send(void const *const data, size_t len);

/**
 * @brief Sets the bluetooth status and handles accordingly
 * 
 * @param ble_event Which state to set bluetooth in
 * 
 * @return 0 if successful
 */
int stream_control_set_status(enum ble_event_t ble_event);

#endif