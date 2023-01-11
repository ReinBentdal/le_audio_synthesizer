#include "button.h"

#include "macros_common.h"

#include <stdint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(button, CONFIG_LOG_BUTTON_LEVEL);

#define BUTTON_EVENTS_MSGQ_MAX_ELEMENTS 3
#define BUTTON_EVENTS_MSGQ_ALIGNMENT_WORDS 4

K_MSGQ_DEFINE(_button_msg_queue, sizeof(struct button_event), BUTTON_EVENTS_MSGQ_MAX_ELEMENTS, BUTTON_EVENTS_MSGQ_ALIGNMENT_WORDS);

static void _button_event_interrupt(const struct device* port, uint8_t button_index);
static inline void _button1_event_interrupt(const struct device* port, struct gpio_callback* cb, uint32_t pin_mask) { _button_event_interrupt(port, 0); }
static inline void _button2_event_interrupt(const struct device* port, struct gpio_callback* cb, uint32_t pin_mask) { _button_event_interrupt(port, 1); }
static inline void _button3_event_interrupt(const struct device* port, struct gpio_callback* cb, uint32_t pin_mask) { _button_event_interrupt(port, 2); }
static inline void _button4_event_interrupt(const struct device* port, struct gpio_callback* cb, uint32_t pin_mask) { _button_event_interrupt(port, 3); }
static inline void _button5_event_interrupt(const struct device* port, struct gpio_callback* cb, uint32_t pin_mask) { _button_event_interrupt(port, 4); }

static void _button_debounce_cb(int index);
static inline void _button1_debounce_done(struct k_timer* timer) { _button_debounce_cb(0); }
static inline void _button2_debounce_done(struct k_timer* timer) { _button_debounce_cb(1); }
static inline void _button3_debounce_done(struct k_timer* timer) { _button_debounce_cb(2); }
static inline void _button4_debounce_done(struct k_timer* timer) { _button_debounce_cb(3); }
static inline void _button5_debounce_done(struct k_timer* timer) { _button_debounce_cb(4); }

static int _button_state_notify(uint8_t index, enum button_state state);

struct button_config {
  const gpio_pin_t pin;
  const uint32_t config_mask;
  const gpio_callback_handler_t event_interrupt;
  const k_timer_expiry_t debounce_cb;

  bool debounce_ongoing;
  enum button_state state;
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

static const struct device* _gpio_nrf5340_audio_dk = DEVICE_DT_GET(DT_NODELABEL(gpio0));

int button_init(void)
{
  __ASSERT_NO_MSG(_gpio_nrf5340_audio_dk != NULL);

  for (int i = 0; i < ARRAY_SIZE(_button_config); i++) {
    int ret;

    ret = gpio_pin_configure(_gpio_nrf5340_audio_dk, _button_config[i].pin, _button_config[i].config_mask);
    RETURN_ON_ERR(ret);

    gpio_init_callback(&_button_callback[i], _button_config[i].event_interrupt, BIT(_button_config[i].pin));

    ret = gpio_add_callback(_gpio_nrf5340_audio_dk, &_button_callback[i]);
    RETURN_ON_ERR(ret);

    ret = gpio_pin_interrupt_configure(_gpio_nrf5340_audio_dk, _button_config[i].pin, GPIO_INT_EDGE_BOTH);
    RETURN_ON_ERR(ret);

    int button_ret = gpio_pin_get(_gpio_nrf5340_audio_dk, _button_config[i].pin);
    if (button_ret < 0) {
      LOG_WRN("Unable to read button state, defaults to BUTTON_RELEASED");
      button_ret = (int)BUTTON_RELEASED;
    }

    _button_config[i].state = (enum button_state)button_ret;
    _button_config[i].debounce_ongoing = false;

    k_timer_init(&_button_config[i].timer, _button_config[i].debounce_cb, NULL);
  }

	return 0;
}

int button_event_get(struct button_event* event, k_timeout_t timeout)
{
  __ASSERT_NO_MSG(event != NULL);

  return k_msgq_get(&_button_msg_queue, (void*)event, timeout);
}

static void _button_event_interrupt(const struct device* port, uint8_t button_index)
{
  __ASSERT(button_index >= 0 && button_index < ARRAY_SIZE(_button_config), "button index out of range");

  if (_button_config[button_index].debounce_ongoing) {
    return;
  }

  /* toggle button state, verify correct state after debounce */
  enum button_state button_toggle_state = (enum button_state)(!(int)(_button_config[button_index].state));
  int ret = _button_state_notify(button_index, button_toggle_state);
	if (ret != 0) {
		LOG_WRN("failed to update button state");
		return;
	}

  _button_config[button_index].debounce_ongoing = true;
  _button_config[button_index].state = button_toggle_state;

  k_timer_start(&_button_config[button_index].timer, K_MSEC(CONFIG_BUTTON_DEBOUNCE_MS), K_NO_WAIT);
}

static void _button_debounce_cb(int index)
{
  __ASSERT(index >= 0 && index < ARRAY_SIZE(_button_config), "button index out of range");

  /* read button state after debounce time to verify correct state */
  int button_ret = gpio_pin_get(_gpio_nrf5340_audio_dk, _button_config[index].pin);
  if (button_ret < 0) {
    LOG_WRN("Unable to read button pin value");
    return;
  }
	
  _button_config[index].debounce_ongoing = false;

  enum button_state button_state = (enum button_state)button_ret;

  if (button_state != _button_config[index].state) {
    LOG_WRN("Registered different button state after debounce");

    int ret = _button_state_notify(index, button_state);
		if (ret != 0) {
			LOG_WRN("failed to update button state after debounce");
			return;
		}

    _button_config[index].state = button_state;
  }
}

static int _button_state_notify(uint8_t index, enum button_state state)
{

  int queue_num_free = k_msgq_num_free_get(&_button_msg_queue);
  const bool queue_full = queue_num_free == 0;
  RETURN_ON_ERR_MSG(queue_full, "Button queue is full, unable to register button event");

  struct button_event button_event = {
    .index = index,
    .state = state,
  };

  int ret = k_msgq_put(&_button_msg_queue, &button_event, K_NO_WAIT);
  RETURN_ON_ERR(ret);

	return 0;
}