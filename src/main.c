/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include "nrfx_clock.h"

#ifdef CONFIG_CPU_LOAD
#include <debug/cpu_load.h>
#endif

#include "audio_process.h"
#include "ble_connection.h"
#include "ble_discovered.h"
#include "button.h"
#include "macros_common.h"
#include "stream_control.h"
#include "synthesizer.h"
#include "led.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_MAIN_LEVEL);

static atomic_t _bt_is_ready = (atomic_t) false;

static void _on_bt_ready(void);
static int _hfclock_config_and_start(void);
static void _button_event_handler(void);

#define BUTTON_EVENT_STACK_SIZE 700
#define BUTTON_EVENT_PRIORITY 5
K_THREAD_DEFINE(_button_event_thread, BUTTON_EVENT_STACK_SIZE, _button_event_handler, NULL, NULL, NULL, BUTTON_EVENT_PRIORITY, 0, 0);

void main(void)
{
  int ret;

  LOG_DBG("nRF5340 APP core started");

#ifdef CONFIG_CPU_LOAD
  cpu_load_init();
#endif


  LOG_DBG("hf clock start");
  ret = _hfclock_config_and_start();
  ERR_CHK_MSG(ret, "failed to start hf clock");

  ret = led_init();
  ERR_CHK_MSG(ret, "failed to initialize leds");

  LOG_DBG("ble discovered init");
  ble_discovered_init();

  LOG_DBG("bluetooth start");
  ret = bluetooth_init(_on_bt_ready);
  ERR_CHK_MSG(ret, "failed to initialize bluetooth");

  /* wait for bluetooth to initialize */
  while (!(bool)atomic_get(&_bt_is_ready)) {
    (void)k_sleep(K_MSEC(100));
  }
  LOG_DBG("bluetooth initialization done");

  LOG_DBG("Audio sync timer init");
  ret = audio_sync_timer_init();
  ERR_CHK_MSG(ret, "failed to initialize audio sync timer");

  LOG_DBG("Audio generate init");
  audio_process_init();

  LOG_DBG("stream control start");
  ret = stream_control_start();
  ERR_CHK_MSG(ret, "failed to start stream control");

  ret = button_init();
  ERR_CHK_MSG(ret, "failed to initialize buttons");

  LOG_DBG("initialization finished");

  while (1) {
    stream_control_event_handler();
    STACK_USAGE_PRINT("main", &z_main_thread);
  }
}


static void _on_bt_ready(void)
{
  (void)atomic_set(&_bt_is_ready, (atomic_t) true);
}

static int _hfclock_config_and_start(void)
{
  int ret;

  /* Use this to turn on 128 MHz clock for cpu_app */
  ret = nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);

  ret -= NRFX_ERROR_BASE_NUM;
  if (ret) {
    return ret;
  }

  nrfx_clock_hfclk_start();
  while (!nrfx_clock_hfclk_is_running()) {
  }

  return 0;
}

static void _button_event_handler(void) {
	while (1) {
		struct button_event event;
		button_event_get(&event, K_FOREVER);

		synthesizer_key_event(&event);
	}
}