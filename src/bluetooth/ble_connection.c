#include "ble_connection.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/byteorder.h>
#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ble, CONFIG_LOG_BLE_LEVEL);

#define NET_CORE_RESPONSE_TIMEOUT_MS 500

/* Note that HCI_CMD_TIMEOUT is currently set to 10 seconds in Zephyr */
#define NET_CORE_WATCHDOG_TIME_MS 1000

static bluetooth_ready_t _on_bt_ready;

static struct k_work _net_core_ctrl_version_get_work;

static void _setup_random_static_address(void);
static void _on_bt_enabled(int err);
static void _net_core_timeout_handler(struct k_timer *timer_id);
static void _net_core_watchdog_handler(struct k_timer *timer_id);
static void _work_net_core_ctrl_version_get(struct k_work *work);
static int _net_core_ctrl_version_get(uint16_t *ctrl_version);

K_TIMER_DEFINE(_net_core_timeout_alarm_timer, _net_core_timeout_handler, NULL);
K_TIMER_DEFINE(_net_core_watchdog_timer, _net_core_watchdog_handler, NULL);


int bluetooth_init(bluetooth_ready_t on_bt_ready) {
    __ASSERT_NO_MSG(on_bt_ready != NULL);

    _on_bt_ready = on_bt_ready;

	/* Setup a timer for monitoring if NET core is working or not */
	k_timer_start(&_net_core_timeout_alarm_timer, K_MSEC(NET_CORE_RESPONSE_TIMEOUT_MS),
		      K_NO_WAIT);

	_setup_random_static_address();
	/* Enable Bluetooth, with callback function that
	 * will be called when Bluetooth is ready
	 */
	int ret = bt_enable(_on_bt_enabled);
	k_timer_stop(&_net_core_timeout_alarm_timer);

	ERR_CHK(ret);

	k_work_init(&_net_core_ctrl_version_get_work, _work_net_core_ctrl_version_get);
	k_timer_start(&_net_core_watchdog_timer, K_NO_WAIT, K_MSEC(NET_CORE_WATCHDOG_TIME_MS));

    return 0;

}

static void _on_bt_enabled(int err) {
    ERR_CHK(err);
    LOG_DBG("bluetooth ready");

    uint16_t ctrl_version = 0;
    int ret = _net_core_ctrl_version_get(&ctrl_version);
	ERR_CHK_MSG(ret, "Failed to get controller version");

	LOG_INF("Controller version: %d", ctrl_version);
	_on_bt_ready();
}

static void _setup_random_static_address(void) {
    int ret;
	static bt_addr_le_t addr;

	if ((NRF_FICR->INFO.DEVICEID[0] != UINT32_MAX) ||
	    ((NRF_FICR->INFO.DEVICEID[1] & UINT16_MAX) != UINT16_MAX)) {
		/* Put the device ID from FICR into address */
		sys_put_le32(NRF_FICR->INFO.DEVICEID[0], &addr.a.val[0]);
		sys_put_le16(NRF_FICR->INFO.DEVICEID[1], &addr.a.val[4]);

		/* The FICR value is a just a random number, with no knowledge
		 * of the Bluetooth Specification requirements for random
		 * static addresses.
		 */
		BT_ADDR_SET_STATIC(&addr.a);

		addr.type = BT_ADDR_LE_RANDOM;

		ret = bt_id_create(&addr, NULL);
		if (ret) {
			LOG_WRN("Failed to create ID");
		}
	} else {
		LOG_WRN("Unable to read from FICR");
		/* If no address can be created based on FICR,
		 * then a random address is created
		 */
	}
}

static void _work_net_core_ctrl_version_get(struct k_work *work)
{
	int ret;
	uint16_t ctrl_version = 0;

	ret = _net_core_ctrl_version_get(&ctrl_version);

	ERR_CHK_MSG(ret, "Failed to get controller version");

	if (!ctrl_version) {
		ERR_CHK_MSG(-EIO, "Failed to contact net core");
	}
}

static int _net_core_ctrl_version_get(uint16_t *ctrl_version)
{
	int ret;
	struct net_buf *rsp;

	ret = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LOCAL_VERSION_INFO, NULL, &rsp);
	if (ret) {
		return ret;
	}

	struct bt_hci_rp_read_local_version_info *rp = (void *)rsp->data;

	*ctrl_version = sys_le16_to_cpu(rp->hci_revision);

	net_buf_unref(rsp);

	return 0;
}

static void _net_core_timeout_handler(struct k_timer *timer_id)
{
	ERR_CHK_MSG(-EIO, "No response from NET core, check if NET core is programmed");
}

static void _net_core_watchdog_handler(struct k_timer *timer_id)
{
	k_work_submit(&_net_core_ctrl_version_get_work);
}