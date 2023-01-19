/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "ble_acl_common.h"
#include "ble_transmit.h"
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <ctype.h>
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/iso.h>
#include "ble_audio_services.h"
#include "ble_acl_gateway.h"
#include "ble_hci_vsc.h"
#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ble, CONFIG_LOG_BLE_LEVEL);

#define BASE_10 10

/* Connection to the peer device - the other nRF5340 Audio device
 * This is the device we are streaming audio to/from.
 */
static struct bt_conn *_p_conn_peer;

K_WORK_DEFINE(_scan_work, work_scan_start);

static void _on_connected_cb(struct bt_conn *conn, uint8_t err);
static void _on_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static bool _conn_parameter_request_cb(struct bt_conn *conn, struct bt_le_conn_param *param);
static void _conn_parameter_updated_cb(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout);
#if (CONFIG_BT_SMP)
static void _security_changed_cb(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
#endif

/**@brief BLE connection callback handlers.
 */
static struct bt_conn_cb _conn_callbacks = {
	.connected = _on_connected_cb,
	.disconnected = _on_disconnected_cb,
	.le_param_req = _conn_parameter_request_cb,
	.le_param_updated = _conn_parameter_updated_cb,
#if (CONFIG_BT_SMP)
	.security_changed = _security_changed_cb,
#endif /* (CONFIG_BT_SMP) */
};


void ble_acl_common_conn_peer_set(struct bt_conn *conn)
{
	__ASSERT_NO_MSG(conn != NULL);
	_p_conn_peer = conn;
}

struct bt_conn* ble_acl_common_conn_peer_get(void)
{
	return _p_conn_peer;
}

void ble_acl_common_start(void)
{
	k_work_submit(&_scan_work);
}

int ble_acl_common_init(void)
{
	bt_conn_cb_register(&_conn_callbacks);

	/* TODO: Initialize BLE services here, SMP is required*/
	return ble_vcs_client_init();
}

/**@brief BLE connected handler.
 */
static void _on_connected_cb(struct bt_conn *conn, uint8_t err)
{
	int ret;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	ARG_UNUSED(ret);
	if (err) {
		LOG_ERR("ACL connection to %s failed, error %d", addr, err);
	} else {
		/* ACL connection established */
		LOG_DBG("ACL connection to %s established", addr);
		/* Setting TX power for connection if set to anything but 0 */
		#if (CONFIG_BLE_CONN_TX_POWER_DBM)
		ret = bt_hci_get_conn_handle(conn, &peer_conn_handle);
		if (ret) {
			LOG_ERR("Unable to get handle, ret = %d", ret);
		} else {
			ret = ble_hci_vsc_set_conn_tx_pwr(peer_conn_handle,
								CONFIG_BLE_CONN_TX_POWER_DBM);
			ERR_CHK(ret);
		}
		#endif

	ble_acl_gateway_on_connected(conn);
#if (CONFIG_BT_SMP)
	ret = bt_conn_set_security(conn, BT_SECURITY_L2);
	ERR_CHK_MSG(ret, "Failed to set security");
#else
	if (!ble_acl_gateway_all_links_connected()) {
		k_work_submit(&_scan_work);
	} else {
		LOG_INF("All ACL links are connected");
		bt_le_scan_stop();
	}
	ret = ble_trans_iso_cis_connect(conn);
	ERR_CHK_MSG(ret, "Failed to connect to ISO CIS channel");
#endif /* (CONFIG_BT_SMP) */
	}
}

/**@brief BLE disconnected handler.
 */
static void _on_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_DBG("ACL disconnected with %s reason: %d", addr, reason);

	struct bt_conn *conn_active;

	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		int ret;

		ret = ble_acl_gateway_conn_peer_get(i, &conn_active);
		ERR_CHK_MSG(ret, "Connection peer get error");
		if (conn_active == conn) {
			bt_conn_unref(conn_active);
			conn_active = NULL;
			ret = ble_acl_gateway_conn_peer_set(i, &conn_active);
			ERR_CHK_MSG(ret, "Connection peer set error");
			LOG_DBG("Headset %d disconnected", i);

			ble_acl_gateway_on_disconnected(conn);
			break;
		}
	}
	k_work_submit(&_scan_work);
}

/**@brief BLE connection parameter request handler.
 */
static bool _conn_parameter_request_cb(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	/* Connection between two nRF5340 Audio DKs are fixed */
	if ((param->interval_min != CONFIG_BLE_ACL_CONN_INTERVAL) ||
	    (param->interval_max != CONFIG_BLE_ACL_CONN_INTERVAL)) {
		return false;
	}

	return true;
}

/**@brief BLE connection parameters updated handler.
 */
static void _conn_parameter_updated_cb(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	LOG_DBG("conn: %x04, int: %d", (int)conn, interval);
	LOG_DBG("lat: %d, to: %d", latency, timeout);

	if (interval <= CONFIG_BLE_ACL_CONN_INTERVAL) {
		/* Link could be ready, since there's no service discover now */
		LOG_DBG("Connection parameters updated");
	}
}

#if (CONFIG_BT_SMP)
static void _security_changed_cb(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	int ret;

	if (err) {
		LOG_ERR("Security failed: level %u err %d", level, err);
		ret = bt_conn_disconnect(conn, err);
		if (ret) {
			LOG_ERR("Failed to disconnect %d", ret);
		}
	} else {
		LOG_DBG("Security changed: level %u", level);
	}


	ret = ble_acl_gateway_mtu_exchange(conn);
	if (ret) {
		LOG_WRN("MTU exchange procedure failed = %d", ret);
	}
}
#endif /* (CONFIG_BT_SMP) */