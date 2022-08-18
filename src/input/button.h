/**
 * @file button.h
 * @author Rein Gundersen Bentdal (rein.bent@gmail.com)
 * @brief handles button/key input read with debounce. Preemtly assumes button state as toggled on 
 *  interrupt with actual state verified after debounce time. This reduces the latency by the debounce time but may
 *  sometimes result in the preemtive value being wrong.
 * @date 2022-08-18
 * 
 * @copyright Copyright (c) 2022
 */

#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <stdint.h>
#include <zephyr/kernel.h>

enum button_state {BUTTON_RELEASED = 0, BUTTON_PRESSED = 1};

struct button_event {
    uint8_t index;
    enum button_state state;
};

int button_init(void);

/**
 * @brief Should be called on a regular basis to prevent buffer overflow
 * 
 * @return error code 
 */
int button_event_get(struct button_event* event, k_timeout_t timeout);

#endif