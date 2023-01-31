#include "led.h"

#include <zephyr/drivers/gpio.h>


#include "macros_common.h"

static const struct gpio_dt_spec _leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};

#define POWER_LED 2
#define LEFT_HEADSET_LED 0
#define RIGHT_HEADSET_LED 1

int led_init(void) {
    const bool ready = device_is_ready(_leds->port);
    RETURN_ON_ERR(!ready);

    for (int i = 0; i < ARRAY_SIZE(_leds); i++) {
	    gpio_pin_configure_dt(&_leds[i], GPIO_OUTPUT_INACTIVE);
    }

    /* indicate power by led */
    gpio_pin_set_dt(&_leds[POWER_LED], 1);

    return 0;
}

void led_headset_connected(uint32_t channel, bool is_connected) {
    __ASSERT(channel < 2, "only support left and right channel");

    if (channel == 0) {
        gpio_pin_set_dt(&_leds[LEFT_HEADSET_LED], (int)is_connected);
    } else {
        gpio_pin_set_dt(&_leds[RIGHT_HEADSET_LED], (int)is_connected);
    }
}