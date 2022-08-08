/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/debug/stack.h>
#include <zephyr/device.h>
#include <string.h>

#include "macros_common.h"
#include "nrfx_clock.h"
#include "ble_connection.h"
#include "stream_control.h"
#include "ble_discovered.h"
#include "audio_codec.h"
#include "key_assign.h"
#include "button.h"
#ifdef CONFIG_CPU_LOAD
#include <debug/cpu_load.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_MAIN_LEVEL);

static atomic_t _bt_is_ready = (atomic_t)false;

static void _on_bt_ready(void);
static int _hfclock_config_and_start(void);


void main(void)
{
	int ret;

	LOG_DBG("nRF5340 APP core started");

#ifdef CONFIG_CPU_LOAD
	cpu_load_init();
#endif

	LOG_DBG("ble discovered init");
	ble_discovered_init();

	LOG_DBG("hf clock start");
	ret = _hfclock_config_and_start();
	ERR_CHK(ret);
	LOG_DBG("hf clock start done");

	LOG_DBG("bluetooth start");
	ret = bluetooth_init(_on_bt_ready);

	/* wait for bluetooth to initialize */
	while (!(bool)atomic_get(&_bt_is_ready)) {
		(void)k_sleep(K_MSEC(100));
	}
	LOG_DBG("bluetooth start done");

	audio_codec_init();

	LOG_DBG("stream control start");
	stream_control_start();
	LOG_DBG("stream control start");	

	button_init();

	LOG_DBG("initialization finished");

	while(1) {
		stream_control_event_handler();
		STACK_USAGE_PRINT("main", &z_main_thread);
	}
}


static void _on_bt_ready(void) {
	(void)atomic_set(&_bt_is_ready, (atomic_t)true);
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
