/**
 * @file led.h
 * @author Rein Gundersen Bentdal
 * @brief Simple interface for controlling board leds
 * @date 2023-01-19
 *
 * 
 * Copyright (c) 2021-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>
#include <stdint.h>

int led_init(void);

void led_headset_connected(uint32_t channel, bool is_connected);
#endif