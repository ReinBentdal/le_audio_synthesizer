/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _BLE_CONNECTION_H_
#define _BLE_CONNECTION_H_

#include <zephyr/kernel.h>

typedef void (*bluetooth_ready_t)(void);

/**@brief	Initialize BLE subsystem
 *
 * @return	0 for success, error otherwise
 */
int bluetooth_init(bluetooth_ready_t bt_on_ready);

#endif