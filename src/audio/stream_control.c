#include "stream_control.h"

#include <zephyr/kernel.h>
#include "ble_transmit.h"
#include "data_fifo.h"
#include "audio_process.h"
#include "audio_sync_timer.h"
#include "ble_hci_vsc.h"
#include "ble_acl_common.h"
#include "macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stream_control, CONFIG_LOG_STREAMCTRL_LEVEL);

#define CONTROL_EVENTS_MSGQ_MAX_ELEMENTS 3
#define CONTROL_EVENTS_MSGQ_ALIGNMENT_WORDS 4

static enum stream_state _stream_state;

K_MSGQ_DEFINE(_ble_msg_queue, sizeof(enum ble_event_t), CONTROL_EVENTS_MSGQ_MAX_ELEMENTS, CONTROL_EVENTS_MSGQ_ALIGNMENT_WORDS);

struct ble_iso_data {
	uint8_t data[CONFIG_BT_ISO_RX_MTU];
	size_t data_size;
	bool bad_frame;
	uint32_t sdu_ref;
	uint32_t recv_frame_ts;
} __packed;

DATA_FIFO_DEFINE(_ble_fifo_rx, CONFIG_BUF_BLE_RX_PACKET_NUM, WB_UP(sizeof(struct ble_iso_data)));

static int _ble_transport_init(void);
#if (CONFIG_TRANSPORT_CIS)
static void _ble_iso_rx_data_handler(uint8_t const *const p_data, size_t data_size, bool bad_frame, uint32_t sdu_ref);
#endif


enum stream_state stream_control_state_get(void) {
    return _stream_state;
}

int stream_control_start(void) {
    	int ret;

	ret = data_fifo_init(&_ble_fifo_rx);
	ERR_CHK_MSG(ret, "Failed to set up ble_rx FIFO");

	ret = _ble_transport_init();
	ERR_CHK(ret);

	_stream_state = STATE_CONNECTING;

	return 0;
}

void stream_control_event_handler(void) {

    /* wait forever for message => no possible return error */
    enum ble_event_t event;
    (void)k_msgq_get(&_ble_msg_queue, (void *)&event, K_FOREVER);

    LOG_DBG("Received event = %d, current state = %d", event, _stream_state);

    switch (event)
    {
    case BLE_EVT_CONNECTED:
        LOG_INF("BLE evt connected");
        if (_stream_state == STATE_DISCONNECTED || _stream_state == STATE_CONNECTING) {
            _stream_state = STATE_CONNECTED;
        }
        /* code */
        break;
    case BLE_EVT_DISCONNECTED:
        LOG_INF("BLE evt disconnected");
        if (_stream_state == STATE_CONNECTED || _stream_state == STATE_LINK_READY) {
            _stream_state = STATE_DISCONNECTED;
        } 
        
        #if ((CONFIG_AUDIO_DEV == GATEWAY) && CONFIG_TRANSPORT_CIS)
        else if (_stream_state == STATE_STREAMING) {
            _stream_state = STATE_DISCONNECTED;
            audio_process_stop();
        }
        #endif /* ((CONFIG_AUDIO_DEV == GATEWAY) && CONFIG_TRANSPORT_CIS) */

        else if (_stream_state == STATE_PAUSED) {
            _stream_state = STATE_DISCONNECTED;
        }

        else {
            LOG_ERR("Got disconnected in state: %d", _stream_state);
        }
        break;
    case BLE_EVT_LINK_READY:
        LOG_INF("BLE evt link ready");
        if (_stream_state == STATE_CONNECTED) {
            audio_process_start();
            _stream_state = STATE_STREAMING;
        }
        break;
    case BLE_EVT_STREAMING:
        LOG_INF("BLE evt streaming");
        break;
    default:
        LOG_WRN("Unexpected/unhandled event: %d", event);
        break;
    }
}

int stream_control_set_status(enum ble_event_t ble_event) {
    if (k_msgq_num_free_get(&_ble_msg_queue) == 0) {
        LOG_WRN("tried to insert control event in a full queue");
        return -ENOMEM;
	}

    k_msgq_put(&_ble_msg_queue, (const void*)(&ble_event), K_NO_WAIT);

    return 0;
}

void stream_control_encoded_data_send(void const *const data, size_t len) {
	int ret;
	static int prev_ret;

    /* only send data if in streaming state */
	if (_stream_state == STATE_STREAMING) {
		ret = ble_trans_iso_tx(data, len, BLE_TRANS_CHANNEL_STEREO);
		if (ret != 0 && ret != prev_ret) {
			LOG_WRN("Problem with sending BLE data, ret: %d", ret);
		}
		prev_ret = ret;
	}
}

static int _ble_transport_init(void) {
    	int ret;

	ret = ble_trans_iso_lost_notify_enable();
	if (ret) {
		return ret;
	}

	/* Setting TX power for advertising if config is set to anything other than 0 */
	if (CONFIG_BLE_ADV_TX_POWER_DBM) {
		ret = ble_hci_vsc_set_adv_tx_pwr(CONFIG_BLE_ADV_TX_POWER_DBM);
		ERR_CHK(ret);
	}

#if (CONFIG_TRANSPORT_BIS)

	ret = ble_trans_iso_init(TRANS_TYPE_BIS, DIR_TX, NULL);
	if (ret) {
		return ret;
	}
	ret = ble_trans_iso_start();
	if (ret) {
		return ret;
	}

#elif (CONFIG_TRANSPORT_CIS)

// #if (!CONFIG_BLE_LE_POWER_CONTROL_ENABLED)
// 	ret = ble_core_le_pwr_ctrl_disable();
// 	if (ret) {
// 		return ret;
// 	}
// #endif /* (!CONFIG_BLE_LE_POWER_CONTROL_ENABLED) */
	ret = ble_acl_common_init();
	if (ret) {
		return ret;
	}
	ble_acl_common_start();
#if (CONFIG_STREAM_BIDIRECTIONAL)

	ret = ble_trans_iso_init(TRANS_TYPE_CIS, DIR_BIDIR, _ble_iso_rx_data_handler);

#else

	ret = ble_trans_iso_init(TRANS_TYPE_CIS, DIR_TX, _ble_iso_rx_data_handler);

#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */
	if (ret) {
		return ret;
	}

	ret = ble_trans_iso_cig_create();
	if (ret) {
		return ret;
	}
#endif /* (CONFIG_TRANSPORT_BIS) */
	return 0;
}

#if (CONFIG_TRANSPORT_CIS)
static void _ble_iso_rx_data_handler(uint8_t const *const p_data, size_t data_size, bool bad_frame,
				    uint32_t sdu_ref)
{
	/* Capture timestamp of when audio frame is received */
	uint32_t recv_frame_ts = audio_sync_timer_curr_time_get();

	/* Since the audio datapath thread is preemptive, no actions on the
	 * FIFO can happen whilst in this handler.
	 */

	if (_stream_state != STATE_STREAMING) {
		/* Throw away data */
		LOG_DBG("Not in streaming state, throwing data: %d", _stream_state);
		return;
	}

	int ret;
	struct ble_iso_data *iso_received = NULL;

#if (CONFIG_BLE_ISO_TEST_PATTERN)
	ble_test_pattern_receive(p_data, data_size, bad_frame);
	return;
#endif /* (CONFIG_BLE_ISO_TEST_PATTERN) */

	uint32_t blocks_alloced_num, blocks_locked_num;

	ret = data_fifo_num_used_get(&_ble_fifo_rx, &blocks_alloced_num, &blocks_locked_num);
	ERR_CHK(ret);

	if (blocks_alloced_num >= CONFIG_BUF_BLE_RX_PACKET_NUM) {
		/* FIFO buffer is full, swap out oldest frame for a new one */

		void *stale_data;
		size_t stale_size;

		LOG_WRN("BLE ISO RX overrun");

		ret = data_fifo_pointer_last_filled_get(&_ble_fifo_rx, &stale_data, &stale_size,
							K_NO_WAIT);
		ERR_CHK(ret);

		ret = data_fifo_block_free(&_ble_fifo_rx, &stale_data);
		ERR_CHK(ret);
	}

	ret = data_fifo_pointer_first_vacant_get(&_ble_fifo_rx, (void *)&iso_received, K_NO_WAIT);
	ERR_CHK_MSG(ret, "Unable to get FIFO pointer");

	if (data_size > ARRAY_SIZE(iso_received->data)) {
		ERR_CHK_MSG(-ENOMEM, "Data size too large for buffer");
	}

	memcpy(iso_received->data, p_data, data_size);

	iso_received->bad_frame = bad_frame;
	iso_received->data_size = data_size;
	iso_received->sdu_ref = sdu_ref;
	iso_received->recv_frame_ts = recv_frame_ts;

	ret = data_fifo_block_lock(&_ble_fifo_rx, (void *)&iso_received,
				   sizeof(struct ble_iso_data));
	ERR_CHK_MSG(ret, "Failed to lock block");
}
#endif /* (CONFIG_TRANSPORT_CIS) */