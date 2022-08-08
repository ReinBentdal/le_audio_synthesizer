#include "button.h"

#include "macros_common.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(button, CONFIG_LOG_BUTTON_LEVEL);


#define BUTTON_EVENTS_MSGQ_MAX_ELEMENTS 3
#define BUTTON_EVENTS_MSGQ_ALIGNMENT_WORDS 4

K_MSGQ_DEFINE(_button_msg_queue, sizeof(struct button_event), BUTTON_EVENTS_MSGQ_MAX_ELEMENTS, BUTTON_EVENTS_MSGQ_ALIGNMENT_WORDS);

static void _button_event_interrupt(const struct device *port, uint8_t button_index);
static inline void _button1_event_interrupt(const struct device *port, struct gpio_callback *cb, uint32_t pin_mask) { _button_event_interrupt(port, 0); }
static inline void _button2_event_interrupt(const struct device *port, struct gpio_callback *cb, uint32_t pin_mask) { _button_event_interrupt(port, 1); }
static inline void _button3_event_interrupt(const struct device *port, struct gpio_callback *cb, uint32_t pin_mask) { _button_event_interrupt(port, 2); }
static inline void _button4_event_interrupt(const struct device *port, struct gpio_callback *cb, uint32_t pin_mask) { _button_event_interrupt(port, 3); }
static inline void _button5_event_interrupt(const struct device *port, struct gpio_callback *cb, uint32_t pin_mask) { _button_event_interrupt(port, 4); }

static void _button_debounce_done(int index);
static inline void _button1_debounce_done(struct k_timer *timer) { _button_debounce_done(0); }
static inline void _button2_debounce_done(struct k_timer *timer) { _button_debounce_done(1); }
static inline void _button3_debounce_done(struct k_timer *timer) { _button_debounce_done(2); }
static inline void _button4_debounce_done(struct k_timer *timer) { _button_debounce_done(3); }
static inline void _button5_debounce_done(struct k_timer *timer) { _button_debounce_done(4); }

struct button_config {
	const gpio_pin_t pin;
	const uint32_t config_mask;
    const gpio_callback_handler_t event_interrupt;
    const k_timer_expiry_t debounce_cb;

    bool debounce_ongoing;
    struct k_timer timer;
};

static struct button_config _button_config[] = {
	{
		.pin = DT_GPIO_PIN(DT_ALIAS(sw0), gpios),
		.config_mask = DT_GPIO_FLAGS(DT_ALIAS(sw0), gpios),
        .event_interrupt = _button1_event_interrupt,
        .debounce_cb = _button1_debounce_done,
	},
	{
		.pin = DT_GPIO_PIN(DT_ALIAS(sw1), gpios),
		.config_mask = DT_GPIO_FLAGS(DT_ALIAS(sw1), gpios),
        .event_interrupt = _button2_event_interrupt,
        .debounce_cb = _button2_debounce_done,
	},
	{
		.pin = DT_GPIO_PIN(DT_ALIAS(sw2), gpios),
		.config_mask = DT_GPIO_FLAGS(DT_ALIAS(sw2), gpios),
        .event_interrupt = _button3_event_interrupt,
        .debounce_cb = _button3_debounce_done,
	},
	{
		.pin = DT_GPIO_PIN(DT_ALIAS(sw3), gpios),
		.config_mask = DT_GPIO_FLAGS(DT_ALIAS(sw3), gpios),
        .event_interrupt = _button4_event_interrupt,
        .debounce_cb = _button4_debounce_done,
	},
	{
		.pin = DT_GPIO_PIN(DT_ALIAS(sw4), gpios),
		.config_mask = DT_GPIO_FLAGS(DT_ALIAS(sw4), gpios),
        .event_interrupt = _button5_event_interrupt,
        .debounce_cb = _button5_debounce_done,
	}
};

static struct gpio_callback _button_callback[ARRAY_SIZE(_button_config)];

static const struct device *_gpio_nrf53_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

int button_init(void) {
    __ASSERT_NO_MSG(_gpio_nrf53_dev != NULL);

    for (int i = 0; i < ARRAY_SIZE(_button_config); i++) {
        int ret;

        ret = gpio_pin_configure(_gpio_nrf53_dev, _button_config[i].pin, _button_config[i].config_mask);
        RETURN_ON_ERR(ret);

        gpio_init_callback(&_button_callback[i], _button_config[i].event_interrupt, BIT(_button_config[i].pin));

        ret = gpio_add_callback(_gpio_nrf53_dev, &_button_callback[i]);
        RETURN_ON_ERR(ret);

        ret = gpio_pin_interrupt_configure(_gpio_nrf53_dev, _button_config[i].pin, GPIO_INT_EDGE_BOTH);
        RETURN_ON_ERR(ret);

        k_timer_init(&_button_config[i].timer, _button_config[i].debounce_cb, NULL);
    }
    return 0;
}

int button_event_get(struct button_event* event, k_timeout_t timeout) {
    __ASSERT_NO_MSG(event != NULL);

    return k_msgq_get(&_button_msg_queue, (void*)event, timeout);
}


static void _button_event_interrupt(const struct device *port, uint8_t button_index) {
    __ASSERT(button_index >= 0 && button_index < ARRAY_SIZE(_button_config), "button index out of range");

    /* read early to minimize the chance of a state change */
    const int button_state = gpio_pin_get(port, _button_config[button_index].pin);
    if (button_state < 0) {
        LOG_WRN("Failed to read button state, error %d", button_state);
        return;
    }

    if (_button_config[button_index].debounce_ongoing) {
        return;
    }

    if (k_msgq_num_free_get(&_button_msg_queue) == 0) {
        LOG_WRN("Button queue is full, unable to register button event");
        return;
    }

    LOG_INF("button %d, state %d", button_index, button_state);

    struct button_event button_event = {
        .index = button_index,
        .state = (enum button_state)button_state,
    };

    int ret = k_msgq_put(&_button_msg_queue, &button_event, K_NO_WAIT);
    ERR_CHK_MSG(ret, "unable to register button event");

    k_timer_start(&_button_config[button_index].timer, K_MSEC(CONFIG_BUTTON_DEBOUNCE_MS), K_NO_WAIT);
}

static void _button_debounce_done(int index) {
    __ASSERT(index >= 0 && index < ARRAY_SIZE(_button_config), "button index out of range");

    _button_config[index].debounce_ongoing = false;
}