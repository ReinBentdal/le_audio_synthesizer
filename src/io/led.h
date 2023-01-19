/**
 * @file led.h
 * @author Rein Gundersen Bentdal
 * @brief Interface for controlling board leds
 * @date 2023-01-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _LED_H_
#define _LED_H_

#include <stdbool.h>
#include <stdint.h>

int led_init(void);

void led_headset_connected(uint32_t channel, bool is_connected);
#endif