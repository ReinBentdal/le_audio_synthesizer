/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/audio/vcs.h>
#include <zephyr/bluetooth/audio/mics.h>
#include "macros_common.h"
#include "ble_acl_common.h"
#include "ble_audio_services.h"

#define LOG_LEVEL CONFIG_LOG_AUDIO_SERVICES_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ble_audio_services);

#define VOLUME_DEFAULT 195
#define VOLUME_STEP 16

static struct bt_vcs *_volume_control_service;

static struct bt_vcs *volume_control_service_client_peer[CONFIG_BT_MAX_CONN];

static int _ble_vcs_client_remote_set(uint8_t channel_num);
static void _vcs_discover_cb_handler(struct bt_vcs *vcs, int err, uint8_t vocs_count, uint8_t aics_count);
static void _vcs_state_cb_handler(struct bt_vcs *vcs, int err, uint8_t volume, uint8_t mute);
static void _vcs_flags_cb_handler(struct bt_vcs *vcs, int err, uint8_t flags);

int ble_vcs_vol_set(uint8_t volume)
{
	int ret;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		ret = _ble_vcs_client_remote_set(i);
		/* If remote peer hasn't been connected before, just skip the operation for it */
		if (ret == -EINVAL) {
			continue;
		}
		ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
		ret = bt_vcs_vol_set(_volume_control_service, volume);
		if (ret) {
			LOG_WRN("Failed to set volume for remote channel %d, ret = %d", i, ret);
		}
	}
	return 0;
}

int ble_vcs_volume_up(void)
{
	int ret;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		ret = _ble_vcs_client_remote_set(i);
		/* If remote peer hasn't been connected before, just skip the operation for it */
		if (ret == -EINVAL) {
			continue;
		}
		ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
		ret = bt_vcs_unmute_vol_up(_volume_control_service);
		if (ret) {
			LOG_WRN("Failed to volume up for remote channel %d, ret = %d", i, ret);
		}
	}
	return 0;
}

int ble_vcs_volume_down(void)
{
	int ret;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		ret = _ble_vcs_client_remote_set(i);
		/* If remote peer hasn't been connected before, just skip the operation for it */
		if (ret == -EINVAL) {
			continue;
		}
		ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
		ret = bt_vcs_unmute_vol_down(_volume_control_service);
		if (ret) {
			LOG_WRN("Failed to volume down for remote channel %d, ret = %d", i, ret);
		}
	}
	return 0;
}

int ble_vcs_volume_mute(void)
{
	int ret;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		ret = _ble_vcs_client_remote_set(i);
		/* If remote peer hasn't been connected before, just skip the operation for it */
		if (ret == -EINVAL) {
			continue;
		}
		ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
		ret = bt_vcs_mute(_volume_control_service);
		if (ret) {
			LOG_WRN("Failed to mute for remote channel %d, ret = %d", i, ret);
		}
	}
	return 0;
}

int ble_vcs_volume_unmute(void)
{
	int ret;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		ret = _ble_vcs_client_remote_set(i);
		/* If remote peer hasn't been connected before, just skip the operation for it */
		if (ret == -EINVAL) {
			continue;
		}
		ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
		ret = bt_vcs_unmute(_volume_control_service);
		if (ret) {
			LOG_WRN("Failed to unmute for remote channel %d, ret = %d", i, ret);
		}
	}
	return 0;
}

int ble_vcs_discover(struct bt_conn *conn, uint8_t channel_num)
{
	int ret;

	if (channel_num > CONFIG_BT_MAX_CONN) {
		return -EPERM;
	}
	ret = bt_vcs_discover(conn, &_volume_control_service);
	volume_control_service_client_peer[channel_num] = _volume_control_service;
	return ret;
}

int ble_vcs_client_init(void)
{
	static struct bt_vcs_cb vcs_client_callback;

	vcs_client_callback.discover = _vcs_discover_cb_handler;
	vcs_client_callback.state = _vcs_state_cb_handler;
	vcs_client_callback.flags = _vcs_flags_cb_handler;
	return bt_vcs_client_cb_register(&vcs_client_callback);
}

int ble_vcs_server_init(void)
{
	int ret;
	struct bt_vcs_register_param vcs_param;
	static struct bt_vcs_cb vcs_server_callback;

	vcs_server_callback.state = _vcs_state_cb_handler;
	vcs_server_callback.flags = _vcs_flags_cb_handler;
	vcs_param.cb = &vcs_server_callback;
	vcs_param.mute = BT_VCS_STATE_UNMUTED;
	vcs_param.step = VOLUME_STEP;
	vcs_param.volume = VOLUME_DEFAULT;

	ret = bt_vcs_register(&vcs_param, &_volume_control_service);
	if (ret) {
		return ret;
	}

	return 0;
}

/**
 * @brief  Callback handler for VCS discover finished
 *
 *         This callback handler will be triggered when VCS
 *         discover is finished.
 */
static void _vcs_discover_cb_handler(struct bt_vcs *vcs, int err, uint8_t vocs_count, uint8_t aics_count)
{
	if (err) {
		LOG_ERR("VCS discover finished callback error: %d", err);
	} else {
		LOG_DBG("VCS discover finished");
	}
}

static int _ble_vcs_client_remote_set(uint8_t channel_num)
{
	if (channel_num > CONFIG_BT_MAX_CONN) {
		return -EPERM;
	}

	if (volume_control_service_client_peer[channel_num] == NULL) {
		return -EINVAL;
	}

	LOG_DBG("VCS client pointed to remote device[%d] %p", channel_num,
		(void *)(volume_control_service_client_peer[channel_num]));
	_volume_control_service = volume_control_service_client_peer[channel_num];
	return 0;
}

/**
 * @brief  Callback handler for volume state changed.
 *
 *         This callback handler will be triggered if
 *         volume state changed, or muted/unmuted.
 */
static void _vcs_state_cb_handler(struct bt_vcs *vcs, int err, uint8_t volume, uint8_t mute)
{
	int ret;

	if (err) {
		LOG_ERR("VCS state callback error: %d", err);
		return;
	}
	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (vcs == volume_control_service_client_peer[i]) {
			LOG_INF("VCS state from remote device %d:", i);
		} else {
			ret = _ble_vcs_client_remote_set(i);
			/* If remote peer hasn't been connected before,
			 * just skip the operation for it
			 */
			if (ret == -EINVAL) {
				continue;
			}
			ERR_CHK_MSG(ret, "Failed to set VCS client to remote device properly");
			LOG_DBG("Sync with other devices %d", i);
			ret = bt_vcs_vol_set(volume_control_service_client_peer[i], volume);
			if (ret) {
				LOG_DBG("Failed to sync volume to remote device %d, err = %d", i,
					ret);
			}
		}
	}
	LOG_INF("Volume = %d, mute state = %d", volume, mute);
}

/**
 * @brief  Callback handler for VCS flags changed.
 *
 *         This callback handler will be triggered if
 *         VCS flags changed.
 */
static void _vcs_flags_cb_handler(struct bt_vcs *vcs, int err, uint8_t flags)
{
	if (err) {
		LOG_ERR("VCS flag callback error: %d", err);
	} else {
		LOG_INF("Volume flags = 0x%01X", flags);
	}
}