/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "ble_transmit.h"

#include "macros_common.h"
#include "sw_codec_select.h"
#include "ble_hci_vsc.h"
#include "audio_sync_timer.h"
#include "ble_acl_common.h"
#include "ble_acl_gateway.h"
#include "stream_control.h"

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/byteorder.h>
#include "host/conn_internal.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ble, CONFIG_LOG_BLE_LEVEL);

static enum iso_transport _iso_transmit_type;
static enum iso_direction _iso_dir;

#define BLE_ISO_PAYLOAD_SIZE_MAX ENC_MAX_FRAME_SIZE

#define BLE_ISO_MTU (BT_ISO_CHAN_SEND_RESERVE + BLE_ISO_PAYLOAD_SIZE_MAX)
#define BLE_ISO_CONN_INTERVAL_US (BLE_ISO_CONN_INTERVAL * 1250)
#define BLE_ISO_LATENCY_MS 10
#define BLE_ISO_RETRANSMITS 2

#define CIS_ISO_CHAN_COUNT CONFIG_BT_ISO_MAX_CHAN
#define BIS_ISO_CHAN_COUNT 1
#define BLE_ISO_BIG_SYNC_TIMEOUT 50

#define CIS_CONN_RETRY_TIMES 5
#define HCI_ISO_BUF_ALLOC_PER_CHAN 2

#define NET_BUF_POOL_ITERATE(i, _)                                         \
	NET_BUF_POOL_FIXED_DEFINE(iso_tx_pool_##i, HCI_ISO_BUF_ALLOC_PER_CHAN, \
							  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU), 8, NULL);
#define NET_BUF_POOL_PTR_ITERATE(i, ...) IDENTITY(&iso_tx_pool_##i)

LISTIFY(CONFIG_BT_ISO_MAX_CHAN, NET_BUF_POOL_ITERATE, (;))

static struct net_buf_pool *_iso_tx_pools[] = {LISTIFY(CONFIG_BT_ISO_MAX_CHAN,
													  NET_BUF_POOL_PTR_ITERATE,
													  (, ))};
static struct bt_iso_chan _iso_chan[CONFIG_BT_ISO_MAX_CHAN];
static struct bt_iso_chan *_iso_chan_ptr[CONFIG_BT_ISO_MAX_CHAN];
static atomic_t _iso_tx_pool_alloc[CONFIG_BT_ISO_MAX_CHAN];
static atomic_t _iso_tx_flush;

struct worker_data
{
	uint8_t channel;
	uint8_t retries;
} __aligned(4);

K_MSGQ_DEFINE(_kwork_msgq, sizeof(struct worker_data), CONFIG_BT_ISO_MAX_CHAN, 4);

static K_SEM_DEFINE(_sem_big_cmplt, 0, 1);
static K_SEM_DEFINE(_sem_big_term, 0, 1);
static K_SEM_DEFINE(_sem_per_adv, 0, 1);
static K_SEM_DEFINE(_sem_per_sync, 0, 1);
static K_SEM_DEFINE(_sem_per_big_info, 0, 1);
static K_SEM_DEFINE(_sem_big_sync, 0, 1);

static struct k_work_delayable _iso_cis_conn_work;

#define BT_LE_SCAN_CUSTOM                                                                     \
	BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_PASSIVE, BT_LE_SCAN_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, \
					 BT_GAP_SCAN_FAST_WINDOW)

#define BT_LE_PER_ADV_CUSTOM                                                  \
	BT_LE_PER_ADV_PARAM(BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, \
						BT_LE_PER_ADV_OPT_NONE)

/* Interval * 1.25 to get ms */
#define BT_INTERVAL_TO_MS(interval) ((interval)*5 / 4)
#define PA_RETRY_COUNT 6

/* Values found empirically to avoid race condition when transmitting stereo in separate
 * function calls
 */
#define SYNC_OFFS_WORKAROUND_THRESH_US 1600
#define SYNC_OFFS_WORKAROUND_PAUSE_THRESH_MS ((BLE_ISO_CONN_INTERVAL_US * 3) / 1000)

static bool _per_adv_found;
static bt_addr_le_t _per_addr;
static uint8_t _per_sid;
static uint16_t _per_interval_ms;

static struct bt_le_ext_adv *_adv;
static struct bt_iso_big *_big;
static struct bt_le_per_adv_sync_param _sync_create_param;
static struct bt_le_per_adv_sync *_iso_sync;
static uint32_t _sem_timeout;
static uint32_t _iso_rx_cb_tot;
static uint32_t _iso_rx_cb_bad;

static ble_trans_iso_rx_cb_t _ble_trans_iso_rx_cb;
static struct bt_iso_chan_io_qos _iso_rx_qos;
static struct bt_iso_chan_qos _iso_bis_qos;

static struct bt_iso_chan_io_qos _iso_tx_qos = {
	.sdu = BLE_ISO_PAYLOAD_SIZE_MAX, /* bytes */
	.rtn = BLE_ISO_RETRANSMITS,
	.phy = BT_GAP_LE_PHY_2M,
};

static struct bt_iso_big_create_param _big_create_param = {
	.num_bis = BIS_ISO_CHAN_COUNT,
	.bis_channels = _iso_chan_ptr,
	.interval = BLE_ISO_CONN_INTERVAL_US, /* in microseconds */
	.latency = BLE_ISO_LATENCY_MS,		  /* milliseconds */
	.packing = BT_ISO_PACKING_SEQUENTIAL,
	.framing = BT_ISO_FRAMING_UNFRAMED,
};

static struct bt_iso_big_sync_param _big_sync_param = {
	.num_bis = BIS_ISO_CHAN_COUNT,
	.bis_channels = _iso_chan_ptr,
	.bis_bitfield = (BIT_MASK(BIS_ISO_CHAN_COUNT) << 1),
	.mse = BT_ISO_SYNC_MSE_MIN,
	.sync_timeout = BLE_ISO_BIG_SYNC_TIMEOUT,
};

static struct bt_iso_chan_qos _iso_cis_qos;

static struct bt_iso_cig_param _cis_create_param = {
	.cis_channels = _iso_chan_ptr,
	.num_cis = CIS_ISO_CHAN_COUNT,
	.sca = BT_GAP_SCA_UNKNOWN,
	.packing = BT_ISO_PACKING_SEQUENTIAL,
	.framing = BT_ISO_FRAMING_UNFRAMED,
	.latency = BLE_ISO_LATENCY_MS,
	.interval = BLE_ISO_CONN_INTERVAL_US,
};

static void _sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info);
static void _term_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_term_info *info);
static void _recv_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf);
static void _biginfo_cb(struct bt_le_per_adv_sync *sync, const struct bt_iso_biginfo *biginfo);

static struct bt_le_per_adv_sync_cb _sync_callbacks = {
	.synced = _sync_cb,
	.term = _term_cb,
	.recv = _recv_cb,
	.biginfo = _biginfo_cb,
};

static void _iso_rx_cb(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info, struct net_buf *buf);
static void _iso_connected_cb(struct bt_iso_chan *chan);
static void _iso_disconnected_cb(struct bt_iso_chan *chan, uint8_t reason);
static void _iso_sent_cb(struct bt_iso_chan *chan);

static struct bt_iso_chan_ops _iso_ops = {
	.recv = _iso_rx_cb,
	.connected = _iso_connected_cb,
	.disconnected = _iso_disconnected_cb,
	.sent = _iso_sent_cb,
};

static uint8_t _iso_chan_to_idx(struct bt_iso_chan *chan);
static void _scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf);
static uint8_t _num_iso_cis_connected(void);
static void _iso_rx_stats_handler(struct k_timer *timer);
static int _iso_bis_rx_sync_delete(void);
static int _iso_bis_rx_sync_get(void);
static int _iso_bis_rx_init(void);
static void _iso_bis_rx_cleanup(void);
static int _iso_bis_rx_start(void);
static int _iso_bis_rx_stop(void);
static int _iso_bis_tx_init(void);
static int _iso_bis_tx_start(void);
static int _iso_bis_tx_stop(void);
static int _iso_bis_start(enum iso_direction dir);
static void _work_iso_cis_conn(struct k_work *work);
static bool _is_iso_buffer_full(uint8_t iso_chan_idx);
static bool _is_iso_buffer_empty(uint8_t iso_chan_idx);
static int _iso_tx(uint8_t const *const data, size_t size, uint8_t iso_chan_idx);
static int _iso_tx_pattern(size_t size, uint8_t iso_chan_idx);
static int _iso_tx_data_or_pattern(uint8_t const *const data, size_t size, uint8_t iso_chan_idx);


int ble_trans_iso_lost_notify_enable(void)
{
	return ble_hci_vsc_set_op_flag(BLE_HCI_VSC_OP_ISO_LOST_NOTIFY, 1);
}

int ble_trans_iso_tx(uint8_t const *const data, size_t size, enum ble_trans_chan_type chan_type)
{
	static int64_t m_prev_tx_time;
	int ret = 0;

	/* BIS defaults to channel 0 */
	if (_iso_transmit_type == TRANS_TYPE_BIS)
	{
		ret = _iso_tx_data_or_pattern(data, size, BLE_TRANS_CHANNEL_LEFT);
		if (ret)
		{
			return ret;
		}
		return 0;
	}

	switch (chan_type)
	{
	case BLE_TRANS_CHANNEL_STEREO:
		if (_is_iso_buffer_full(BLE_TRANS_CHANNEL_LEFT) ||
			_is_iso_buffer_full(BLE_TRANS_CHANNEL_RIGHT))
		{
			/* When transmitting stereo,
			 * make sure there is sufficent buffer space for both channels.
			 */
			return -ENOMEM;
		}

		if (atomic_get(&_iso_tx_flush))
		{
			/* Make sure the iso tx buffers are empty before starting streaming to
			 * newly connected device.
			 */
			if (_is_iso_buffer_empty(BLE_TRANS_CHANNEL_LEFT) &&
				_is_iso_buffer_empty(BLE_TRANS_CHANNEL_RIGHT))
			{
				atomic_dec(&_iso_tx_flush);
			}

			return 0;
		}

		if (_num_iso_cis_connected() > 1 &&
			k_uptime_delta(&m_prev_tx_time) > SYNC_OFFS_WORKAROUND_PAUSE_THRESH_MS)
		{
			/* Workaround for lacking sequence number param in iso transmit function:
			 * If the first packet in a streaming session is close to the CIS ISO
			 * anchor point, delay this packet transmission slightly.
			 * Otherwise there is a chance the left data can be sent in
			 * ISO conn interval N, and the right in interval N+1.
			 * This would lead to a permanent offset between the channels.
			 */
			uint32_t sdu_ref_us = 0;
			uint32_t time_now_us = audio_sync_timer_curr_time_get();

			ret = ble_trans_iso_tx_anchor_get(chan_type, &sdu_ref_us, NULL);
			if (ret == -EIO)
			{
				/* The very first call to this function is expected to fail,
				 * as streaming has not yet started.
				 * In this case,
				 * begin audio transmit and check timing on the next packet.
				 */
				atomic_inc(&_iso_tx_flush);
				m_prev_tx_time = 0;
			}
			else if (ret)
			{
				LOG_WRN("ble_trans_iso_tx_anchor_get: %d", ret);
			}

			int diff_balanced_us =
				(time_now_us - sdu_ref_us - BLE_ISO_CONN_INTERVAL_US);
			int sleep_usec;

			if (abs(diff_balanced_us) < SYNC_OFFS_WORKAROUND_THRESH_US)
			{
				/* Anchor point too close */
				sleep_usec = SYNC_OFFS_WORKAROUND_THRESH_US - diff_balanced_us;
				LOG_DBG("diff_balanced_us=%d,sleep_usec=%d", diff_balanced_us,
						sleep_usec);
				k_usleep(sleep_usec);
			}
		}

		ret = _iso_tx_data_or_pattern(data, size / 2, BLE_TRANS_CHANNEL_LEFT);
		if (ret)
		{
			return ret;
		}

		ret = _iso_tx_data_or_pattern(&data[size / 2], size / 2, BLE_TRANS_CHANNEL_RIGHT);
		if (ret)
		{
			return ret;
		}

		break;
	case BLE_TRANS_CHANNEL_RETURN_MONO:
		ret = _iso_tx_data_or_pattern(data, size, BLE_TRANS_CHANNEL_RETURN_MONO);
		if (ret)
		{
			return ret;
		}
		break;
	default:
		return -EPERM;
	}

	return ret;
}

int ble_trans_iso_start(void)
{
	switch (_iso_transmit_type)
	{
	case TRANS_TYPE_NOT_SET:
		LOG_ERR("Transport type not set");
		return -EPERM;
	case TRANS_TYPE_BIS:
		return _iso_bis_start(_iso_dir);
	case TRANS_TYPE_CIS:
		/* The ISO CIS channel established automatically
		 * after ACL connected, so ISO CIS is ready for use.
		 * And once ISO CIS channel established,
		 * we maintain it unless ACL disconnected for now.
		 */
		return 0;
	default:
		return -EPERM;
	};

	return 0;
}

int ble_trans_iso_stop(void)
{
	if (_iso_transmit_type == TRANS_TYPE_BIS)
	{
		switch (_iso_dir)
		{
		case DIR_RX:
			return _iso_bis_rx_stop();
		case DIR_TX:
			return _iso_bis_tx_stop();
		default:
			return -EPERM;
		};
	}
	else if (_iso_transmit_type == TRANS_TYPE_CIS)
	{
		return 0;
	}
	else
	{
		return -EPERM;
	}
}

int ble_trans_iso_cig_create(void)
{
	int ret;

	struct bt_iso_cig *cig;

	ret = bt_iso_cig_create(&_cis_create_param, &cig);
	if (ret)
	{
		LOG_ERR("Failed to create CIG (%d)\n", ret);
		return ret;
	}

	return 0;
}

int ble_trans_iso_cis_connect(struct bt_conn *conn)
{
	int ret;
	struct bt_conn *conn_active;

	for (uint8_t i = 0; i < CONFIG_BT_MAX_CONN; i++)
	{
		ret = ble_acl_gateway_conn_peer_get(i, &conn_active);
		ERR_CHK_MSG(ret, "Connection peer get error");
		if (conn == conn_active)
		{
			struct worker_data work_data;

			work_data.channel = i;
			work_data.retries = 0;

			ret = k_msgq_put(&_kwork_msgq, &work_data, K_NO_WAIT);
			if (ret)
			{
				return ret;
			}

			k_work_schedule(&_iso_cis_conn_work, K_MSEC(500 * i));
		}
	}

	return 0;
}

int ble_trans_iso_tx_anchor_get(enum ble_trans_chan_type chan_type, uint32_t *timestamp,
								uint32_t *offset)
{
	int ret;

	if (timestamp == NULL && offset == NULL)
	{
		return -EINVAL;
	}

	switch (chan_type)
	{
	case BLE_TRANS_CHANNEL_LEFT:
	case BLE_TRANS_CHANNEL_RIGHT:
	case BLE_TRANS_CHANNEL_STEREO:
		break;
	default:
		return -EINVAL;
	}

	if (k_is_in_isr())
	{
		return -EPERM;
	}

	int chan_idx = -1;

	if (chan_type == BLE_TRANS_CHANNEL_STEREO)
	{
		/* Choose the first active connection */
		for (int i = 0; i < ARRAY_SIZE(_iso_chan_ptr); ++i)
		{
			if (_iso_chan_ptr[i]->state == BT_ISO_STATE_CONNECTED)
			{
				chan_idx = i;
				break;
			}
		}
	}
	else if (_iso_chan_ptr[chan_type]->state == BT_ISO_STATE_CONNECTED)
	{
		__ASSERT_NO_MSG(chan_type < ARRAY_SIZE(_iso_chan_ptr));
		chan_idx = chan_type;
	}

	if (chan_idx == -1)
	{
		return -ENOLINK;
	}

	__ASSERT_NO_MSG(_iso_chan_ptr[chan_idx]->iso != NULL);

	uint16_t handle = _iso_chan_ptr[chan_idx]->iso->handle;

	struct bt_hci_cp_le_read_iso_tx_sync *cp;
	struct bt_hci_rp_le_read_iso_tx_sync *rp;
	struct net_buf *buf;
	struct net_buf *rsp = NULL;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_READ_ISO_TX_SYNC, sizeof(*cp));
	if (!buf)
	{
		return -ENOMEM;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);

	ret = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_ISO_TX_SYNC, buf, &rsp);
	if (ret)
	{
		return ret;
	}

	if (rsp)
	{
		rp = (struct bt_hci_rp_le_read_iso_tx_sync *)rsp->data;
		if (timestamp != NULL)
		{
			*timestamp = rp->timestamp;
		}
		if (offset != NULL)
		{
			*offset = sys_get_le24(rp->offset);
		}
		net_buf_unref(rsp);
	}
	else
	{
		return -ENOTSUP;
	}

	return 0;
}

int ble_trans_iso_bis_rx_sync_get(void)
{
	return _iso_bis_rx_sync_get();
}

K_TIMER_DEFINE(iso_rx_stats_timer, _iso_rx_stats_handler, NULL);

int ble_trans_iso_init(enum iso_transport trans_type, enum iso_direction dir,
					   ble_trans_iso_rx_cb_t rx_cb)
{
	int ret;

	if (_iso_dir != DIR_NOT_SET)
	{
		return -EINVAL;
	}

	_iso_dir = dir;

	if (_iso_transmit_type != TRANS_TYPE_NOT_SET)
	{
		return -EPERM;
	}

	for (int8_t i = 0; i < CONFIG_BT_ISO_MAX_CHAN; i++)
	{
		_iso_chan_ptr[i] = &_iso_chan[i];
	}

	_iso_transmit_type = trans_type;

	switch (_iso_transmit_type)
	{
	case TRANS_TYPE_NOT_SET:
		LOG_ERR("NOT SET");
		return -EPERM;
	case TRANS_TYPE_BIS:
		switch (_iso_dir)
		{
		case DIR_RX:
			_iso_bis_qos.rx = &_iso_rx_qos;
			_iso_bis_qos.tx = NULL;

			ret = _iso_bis_rx_init();
			if (ret)
			{
				return ret;
			}

			break;
		case DIR_TX:
			_iso_bis_qos.rx = NULL;
			_iso_bis_qos.tx = &_iso_tx_qos;

			ret = _iso_bis_tx_init();
			if (ret)
			{
				return ret;
			}

			break;
		default:
			return -EPERM;
		};

		_iso_chan_ptr[0]->ops = &_iso_ops;
		_iso_chan_ptr[0]->qos = &_iso_bis_qos;
		break;
	case TRANS_TYPE_CIS:
		k_work_init_delayable(&_iso_cis_conn_work, _work_iso_cis_conn);

		for (int i = 0; i < CIS_ISO_CHAN_COUNT; i++)
		{
			_iso_chan_ptr[i]->ops = &_iso_ops;
			_iso_chan_ptr[i]->qos = &_iso_cis_qos;
		}
		switch (_iso_dir)
		{
		case DIR_RX:
			/* Use the same setting between TX and RX */
			memcpy(&_iso_rx_qos, &_iso_tx_qos, sizeof(_iso_tx_qos));
			_iso_cis_qos.rx = &_iso_rx_qos;
			_iso_cis_qos.tx = NULL;
			break;
		case DIR_TX:
			_iso_cis_qos.rx = NULL;
			_iso_cis_qos.tx = &_iso_tx_qos;
			break;
		case DIR_BIDIR:
			/* Use the same setting between TX and RX */
			memcpy(&_iso_rx_qos, &_iso_tx_qos, sizeof(_iso_tx_qos));
			_iso_cis_qos.rx = &_iso_rx_qos;
			_iso_cis_qos.tx = &_iso_tx_qos;
			break;
		default:
			return -EPERM;
		};
		break;
	default:
		return -EPERM;
	};
	_ble_trans_iso_rx_cb = rx_cb;

	if (CONFIG_BLE_ISO_RX_STATS_S != 0)
	{
		k_timer_start(&iso_rx_stats_timer, K_SECONDS(0),
					  K_SECONDS(CONFIG_BLE_ISO_RX_STATS_S));
	}

	LOG_DBG("Init finished");
	return 0;
}

static uint8_t _iso_chan_to_idx(struct bt_iso_chan *chan)
{
	if (chan == NULL)
	{
		ERR_CHK_MSG(-EINVAL, "chan is NULL");
	}

	for (uint8_t i = 0; i < CONFIG_BT_ISO_MAX_CHAN; i++)
	{
		if (chan == _iso_chan_ptr[i])
		{
			return i;
		}
	}

	ERR_CHK_MSG(-ENXIO, "No index found for this channel");
	CODE_UNREACHABLE;
	return UINT8_MAX;
}

static void _scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	int ret;

	if (!_per_adv_found && info->interval)
	{
		_per_adv_found = true;

		_per_sid = info->sid;
		_per_interval_ms = BT_INTERVAL_TO_MS(info->interval);
		bt_addr_le_copy(&_per_addr, info->addr);

		LOG_INF("Stop scanning...");
		ret = bt_le_scan_stop();
		ERR_CHK_MSG(ret, "bt_le_scan_stop failed");

		ret = stream_control_set_status(BLE_EVT_CONNECTED);
		ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_CONNECTED in event queue");
		k_sem_give(&_sem_per_adv);
	}
}


static struct bt_le_scan_cb scan_callbacks = {
	.recv = _scan_recv,
};

static void _sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info)
{
	k_sem_give(&_sem_per_sync);
}

static void _term_cb(struct bt_le_per_adv_sync *sync,
					 const struct bt_le_per_adv_sync_term_info *info)
{
	int ret;

	LOG_WRN("Lost sync");
	if (_iso_dir == DIR_RX)
	{
		ret = _iso_bis_rx_sync_delete();
		if (ret)
		{
			LOG_WRN("_iso_bis_rx_sync_delete failed, ret = %d", ret);
		}
		ret = stream_control_set_status(BLE_EVT_DISCONNECTED);
		ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_DISCONNECTED in event queue");
	}
}

static void _recv_cb(struct bt_le_per_adv_sync *sync,
					 const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf)
{
	/* Currently not in use */
}

static void _biginfo_cb(struct bt_le_per_adv_sync *sync, const struct bt_iso_biginfo *biginfo)
{
	k_sem_give(&_sem_per_big_info);
}

static uint8_t _num_iso_cis_connected(void)
{
	uint8_t num_cis_connected = 0;

	for (int i = 0; i < CIS_ISO_CHAN_COUNT; i++)
	{
		if (_iso_chan_ptr[i]->state == BT_ISO_STATE_CONNECTED)
		{
			num_cis_connected++;
		}
	}
	return num_cis_connected;
}

static void _iso_rx_stats_handler(struct k_timer *timer)
{
	float bad_pcnt;

	if (_iso_rx_cb_bad == 0)
	{
		bad_pcnt = 0;
	}
	else
	{
		bad_pcnt = 100 * (float)_iso_rx_cb_bad / _iso_rx_cb_tot;
	}
	LOG_WRN("BLE ISO RX. tot: %d bad %d, Percent %1.1f", _iso_rx_cb_tot, _iso_rx_cb_bad,
			bad_pcnt);

	_iso_rx_cb_tot = 0;
	_iso_rx_cb_bad = 0;
}

static void _iso_rx_cb(struct bt_iso_chan *chan, const struct bt_iso_recv_info *info,
					   struct net_buf *buf)
{
	bool bad_frame = false;

	_iso_rx_cb_tot++;

	if (_ble_trans_iso_rx_cb == NULL)
	{
		ERR_CHK_MSG(-EPERM, "The RX callback has not been set");
	}

	if (!(info->flags & BT_ISO_FLAGS_VALID))
	{
		bad_frame = true;
		_iso_rx_cb_bad++;
	}

	_ble_trans_iso_rx_cb(buf->data, buf->len, bad_frame, info->ts);
}

static void _iso_connected_cb(struct bt_iso_chan *chan)
{
	LOG_DBG("ISO Channel %p connected", chan);

	if (_iso_transmit_type == TRANS_TYPE_BIS)
	{
		k_sem_give(&_sem_big_sync);
		k_sem_give(&_sem_big_cmplt);
	}
	else if (_iso_transmit_type == TRANS_TYPE_CIS)
	{
		int ret;

		/* Only send BLE event on first connected CIS */
		if (_num_iso_cis_connected() == 1)
		{
			ret = stream_control_set_status(BLE_EVT_CONNECTED);
			ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_CONNECTED in event queue");

			ret = stream_control_set_status(BLE_EVT_LINK_READY);
			ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_LINK_READY in event queue");
		}
		else
		{
			/* Wait for ongoing transmissions to complete before enqueuing more */
			atomic_inc(&_iso_tx_flush);
		}
	}
	else
	{
		LOG_ERR("_iso_transmit_type error");
	}
}

static void _iso_disconnected_cb(struct bt_iso_chan *chan, uint8_t reason)
{
	int ret;

	atomic_clear(&_iso_tx_pool_alloc[_iso_chan_to_idx(chan)]);

	if (_iso_transmit_type == TRANS_TYPE_BIS)
	{
		k_sem_give(&_sem_big_term);
		if (reason != BT_HCI_ERR_LOCALHOST_TERM_CONN)
		{
			LOG_DBG("Not cancelled by local host");
			if (_iso_dir == DIR_RX)
			{
				ret = _iso_bis_rx_sync_delete();
				if (ret)
				{
					LOG_DBG("_iso_bis_rx_sync_delete failed, ret = %d", ret);
				}
				ret = stream_control_set_status(BLE_EVT_DISCONNECTED);
				ERR_CHK_MSG(
					ret,
					"Unable to put event BLE_EVT_DISCONNECTED in event queue");
			}
		}
	}
	else if (_iso_transmit_type == TRANS_TYPE_CIS)
	{

		for (int i = 0; i < CIS_ISO_CHAN_COUNT; i++)
		{
			if (chan == _iso_chan_ptr[i])
			{
				LOG_DBG("ISO CIS %d disconnected, reason %d", i, reason);
				break;
			}
		}
		/* If there's no ISO CIS channel connected,
		 * trigger BLE_EVT_DISCONNECTED for stopping encoding thread
		 */
		if (_num_iso_cis_connected() == 0)
		{
			ret = stream_control_set_status(BLE_EVT_DISCONNECTED);
			ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_DISCONNECTED in event queue");
		}
	}
	else
	{
		LOG_ERR("_iso_transmit_type not supported");
	}
}

/* Called when ISO TX is done and the buffer
 * has been freed
 */
static void _iso_sent_cb(struct bt_iso_chan *chan)
{
	atomic_dec(&_iso_tx_pool_alloc[_iso_chan_to_idx(chan)]);
}

static int _iso_bis_rx_sync_delete(void)
{
	int ret;

	_per_adv_found = false;

	if (_iso_sync == NULL)
	{
		LOG_DBG("Already deleted _iso_sync");
	}

	LOG_DBG("Deleting Periodic Advertising Sync...");

	ret = bt_le_per_adv_sync_delete(_iso_sync);
	if (ret)
	{
		LOG_ERR("bt_le_per_adv_sync_delete failed: %d", ret);
	}

	_iso_sync = NULL;

	return ret;
}

static int _iso_bis_rx_sync_get(void)
{
	int ret;

	if (_iso_sync != NULL)
	{
		LOG_DBG("Already have sync, returning");
		return 0;
	}

	_per_adv_found = false;
	LOG_DBG("Start scanning...");

	ret = bt_le_scan_start(BT_LE_SCAN_CUSTOM, NULL);
	if (ret)
	{
		LOG_ERR("ble_le_scan_start failed");
		return ret;
	}

	LOG_DBG("Waiting for periodic advertising...");

	ret = k_sem_take(&_sem_per_adv, K_FOREVER);
	if (ret)
	{
		LOG_ERR("k_sem_take failed");
		return ret;
	}

	LOG_DBG("Creating Periodic Advertising Sync...");
	bt_addr_le_copy(&_sync_create_param.addr, &_per_addr);
	_sync_create_param.options = 0;
	_sync_create_param.sid = _per_sid;
	_sync_create_param.skip = 0;
	_sync_create_param.timeout = (_per_interval_ms * PA_RETRY_COUNT) / 10;
	_sem_timeout = _per_interval_ms * PA_RETRY_COUNT;

	ret = bt_le_per_adv_sync_create(&_sync_create_param, &_iso_sync);
	if (ret)
	{
		LOG_ERR("bt_le_per_adv_sync_create failed");
		return ret;
	}

	LOG_DBG("Waiting for periodic sync");
	k_sem_reset(&_sem_per_sync);
	ret = k_sem_take(&_sem_per_sync, K_MSEC(_sem_timeout));
	if (ret)
	{
		LOG_ERR("_sem_per_sync failed");
		return ret;
	}

	LOG_DBG("Periodic sync established");
	k_sem_reset(&_sem_per_big_info);

	ret = k_sem_take(&_sem_per_big_info, K_MSEC(_sem_timeout));
	if (ret)
	{
		LOG_ERR("_sem_per_big_info failed");
		return ret;
	}

	ret = stream_control_set_status(BLE_EVT_LINK_READY);
	ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_LINK_READY in event queue");

	return 0;
}

static int _iso_bis_rx_init(void)
{
	int ret;

	bt_le_scan_cb_register(&scan_callbacks);
	bt_le_per_adv_sync_cb_register(&_sync_callbacks);

	ret = _iso_bis_rx_sync_get();
	if (ret)
	{
		LOG_ERR("Sync lost before enter streaming state");
		return ret;
	}

	return 0;
}

static void _iso_bis_rx_cleanup(void)
{
	int ret;

	ret = _iso_bis_rx_sync_delete();
	if (ret)
	{
		LOG_WRN("Failed to clean sync, ret = %d", ret);
	}

	ret = bt_iso_big_terminate(_big);
	if (ret)
	{
		LOG_WRN("Failed to terminate BIG, ret = %d", ret);
	}
	else
	{
		_big = NULL;
	}
}

static int _iso_bis_rx_start(void)
{
	int ret;

	ret = _iso_bis_rx_sync_get();
	if (ret)
	{
		LOG_WRN("_iso_bis_rx_sync_get failed");
		_iso_bis_rx_sync_delete();
		return ret;
	}

	LOG_DBG("Trying to get sync...");
	/* Set the BIS we want to sync to */

	ret = bt_iso_big_sync(_iso_sync, &_big_sync_param, &_big);
	if (ret)
	{
		LOG_ERR("bt_iso_big_sync failed");
		_iso_bis_rx_cleanup();
		return ret;
	}

	ret = k_sem_take(&_sem_big_sync, K_MSEC(_sem_timeout));
	if (ret)
	{
		LOG_ERR("k_sem_take _sem_big_sync failed");
		_iso_bis_rx_cleanup();
		return ret;
	}

	LOG_DBG("bt_iso_big_sync established");

	ret = stream_control_set_status(BLE_EVT_STREAMING);
	ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_STREAMING in event queue");

	return 0;
}

static int _iso_bis_rx_stop(void)
{
	int ret;

	ret = bt_iso_big_terminate(_big);
	if (ret)
	{
		return ret;
	}

	LOG_DBG("Waiting for BIG terminate complete...");

	ret = k_sem_take(&_sem_big_term, K_FOREVER);
	if (ret)
	{
		return ret;
	}
	LOG_DBG("BIG terminate completed");

	return 0;
}

static int _iso_bis_tx_init(void)
{
	int ret;

	/* Create a non-connectable non-scannable advertising set */
	ret = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN_NAME, NULL, &_adv);
	if (ret)
	{
		LOG_ERR("Failed to create advertising set");
		return ret;
	}

	return 0;
}

static int _iso_bis_tx_start(void)
{
	int ret;

	/* Set periodic advertising parameters */
	ret = bt_le_per_adv_set_param(_adv, BT_LE_PER_ADV_CUSTOM);
	if (ret)
	{
		LOG_ERR("Failed to set periodic advertising parameters");
		return ret;
	}

	/* Enable Periodic Advertising */
	ret = bt_le_per_adv_start(_adv);
	if (ret)
	{
		LOG_ERR("Failed to enable periodic advertising");
		return ret;
	}

	/* Start extended advertising */
	ret = bt_le_ext_adv_start(_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (ret)
	{
		LOG_ERR("Failed to start extended advertising");
		return ret;
	}

	ret = stream_control_set_status(BLE_EVT_CONNECTED);
	ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_CONNECTED in event queue");

	/* Create BIG */
	ret = bt_iso_big_create(_adv, &_big_create_param, &_big);
	if (ret)
	{
		LOG_ERR("Failed to create BIG");
		return ret;
	}

	LOG_DBG("Waiting for BIG complete");
	ret = k_sem_take(&_sem_big_cmplt, K_FOREVER);
	if (ret)
	{
		return ret;
	}

	ret = stream_control_set_status(BLE_EVT_LINK_READY);
	ERR_CHK_MSG(ret, "Unable to put event BLE_EVT_LINK_READY in event queue");

	LOG_DBG("ISO Create done");

	return 0;
}

static int _iso_bis_tx_stop(void)
{
	int ret;

	ret = bt_iso_big_terminate(_big);
	if (ret)
	{
		return ret;
	}

	LOG_DBG("Waiting for BIG terminate complete...");

	ret = k_sem_take(&_sem_big_term, K_MSEC(_sem_timeout));
	if (ret == -EAGAIN)
	{
		LOG_WRN("_sem_big_term timeout");
		return ret;
	}

	LOG_DBG("Stop advertising");

	ret = bt_le_ext_adv_stop(_adv);
	if (ret)
	{
		return ret;
	}

	LOG_DBG("Disable periodic advertising");

	ret = bt_le_per_adv_stop(_adv);
	if (ret)
	{
		return ret;
	}

	return 0;
}

static int _iso_bis_start(enum iso_direction dir)
{
	switch (dir)
	{
	case DIR_RX:
		return _iso_bis_rx_start();
	case DIR_TX:
		return _iso_bis_tx_start();
	default:
		LOG_WRN("Invalid direction supplied: %d", dir);
		return -EPERM;
	};
}

static void _work_iso_cis_conn(struct k_work *work)
{
	int ret;
	struct bt_iso_connect_param connect_param;
	struct worker_data work_data;

	ret = k_msgq_get(&_kwork_msgq, &work_data, K_NO_WAIT);
	ERR_CHK(ret);

	ret = ble_acl_gateway_conn_peer_get(work_data.channel, &connect_param.acl);
	ERR_CHK_MSG(ret, "Connection peer get error");
	connect_param.iso_chan = _iso_chan_ptr[work_data.channel];

	ret = bt_iso_chan_connect(&connect_param, 1);
	work_data.retries++;
	if (ret)
	{
		if (work_data.retries < CIS_CONN_RETRY_TIMES)
		{
			LOG_WRN("Got connect error from ch %d Retrying. code: %d count: %d",
					work_data.channel, ret, work_data.retries);
			ret = k_msgq_put(&_kwork_msgq, &work_data, K_NO_WAIT);
			ERR_CHK(ret);
			/* Delay added to prevent controller overloading */
			k_work_reschedule(&_iso_cis_conn_work, K_MSEC(500));
		}
		else
		{
			LOG_ERR("Could not connect ch %d after %d retries", work_data.channel,
					work_data.retries);
			bt_conn_disconnect(connect_param.acl, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		}
	}
}

static bool _is_iso_buffer_full(uint8_t iso_chan_idx)
{
	/* net_buf_alloc allocates buffers for APP->NET transfer over HCI RPMsg.
	 * The _iso_sent_cb is called when a buffer/buffers have been released,
	 * i.e. data transfer to the NET core has been completed.
	 * Data will be discarded if allocation becomes too high.
	 * If the NET and APP core operates in clock sync, discarding should not occur.
	 */

	if (atomic_get(&_iso_tx_pool_alloc[iso_chan_idx]) >= HCI_ISO_BUF_ALLOC_PER_CHAN)
	{
		return true;
	}
	return false;
}

static bool _is_iso_buffer_empty(uint8_t iso_chan_idx)
{
	if (atomic_get(&_iso_tx_pool_alloc[iso_chan_idx]) == 0)
	{
		return true;
	}
	return false;
}

static int _iso_tx(uint8_t const *const data, size_t size, uint8_t iso_chan_idx)
{
	int ret;
	static bool wrn_printed[CONFIG_BT_ISO_MAX_CHAN];
	struct net_buf *net_buffer;

	if (_is_iso_buffer_full(iso_chan_idx))
	{
		if (!wrn_printed[iso_chan_idx])
		{
			LOG_WRN("HCI ISO TX overrun on ch %d. Single print", iso_chan_idx);
			wrn_printed[iso_chan_idx] = true;
		}
		return -ENOMEM;
	}

	wrn_printed[iso_chan_idx] = false;

	net_buffer = net_buf_alloc(_iso_tx_pools[iso_chan_idx], K_NO_WAIT);
	if (net_buffer == NULL)
	{
		LOG_ERR("No net buf available");
		return -ENOMEM;
	}

	atomic_inc(&_iso_tx_pool_alloc[iso_chan_idx]);
	/* Headroom reserved for stack use */
	net_buf_reserve(net_buffer, BT_ISO_CHAN_SEND_RESERVE);
	net_buf_add_mem(net_buffer, data, size);

	ret = bt_iso_chan_send(_iso_chan_ptr[iso_chan_idx], net_buffer);
	if (ret < 0)
	{
		LOG_ERR("Unable to send ISO data: %d", ret);
		net_buf_unref(net_buffer);
		atomic_dec(&_iso_tx_pool_alloc[iso_chan_idx]);
		return ret;
	}
	return 0;
}

static int _iso_tx_pattern(size_t size, uint8_t iso_chan_idx)
{
	int ret;
	static uint8_t test_pattern_data[CONFIG_BT_ISO_MAX_CHAN][ENC_MAX_FRAME_SIZE];
	static uint8_t test_pattern_value[CONFIG_BT_ISO_MAX_CHAN];

	if (iso_chan_idx >= CONFIG_BT_ISO_MAX_CHAN)
	{
		LOG_ERR("Unknown channel");
		return -EIO;
	}

	memset(test_pattern_data[iso_chan_idx], test_pattern_value[iso_chan_idx],
		   ENC_MAX_FRAME_SIZE);
	ret = _iso_tx(test_pattern_data[iso_chan_idx], size, iso_chan_idx);

	if (ret == 0)
	{
		if (test_pattern_value[iso_chan_idx] == UINT8_MAX)
		{
			test_pattern_value[iso_chan_idx] = 0;
		}
		else
		{
			test_pattern_value[iso_chan_idx]++;
		}
	}

	return ret;
}

static int _iso_tx_data_or_pattern(uint8_t const *const data, size_t size, uint8_t iso_chan_idx)
{
	int ret;

	if (_iso_chan_ptr[iso_chan_idx]->state != BT_ISO_STATE_CONNECTED)
	{
		// LOG_DBG("ISO channel %d not connected", iso_chan_idx);
		return 0;
	}

	if (IS_ENABLED(CONFIG_BLE_ISO_TEST_PATTERN))
	{
		ret = _iso_tx_pattern(size, iso_chan_idx);
	}
	else
	{
		ret = _iso_tx(data, size, iso_chan_idx);
	}

	return ret;
}
