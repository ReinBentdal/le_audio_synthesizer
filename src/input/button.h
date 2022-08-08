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
 * @brief Should be called on a regular basis to prevent full buffer
 * 
 * @return error code 
 */
int button_event_get(struct button_event* event, k_timeout_t timeout);

#endif