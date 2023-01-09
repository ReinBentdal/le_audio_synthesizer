#include "audio_generate.h"

#include <zephyr/kernel.h>
#include "sw_codec_select.h"
#include "macros_common.h"
#include "stream_control.h"
#include "button.h"
#include "synthesizer.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_codec, CONFIG_LOG_AUDIO_CODEC_LEVEL);

#if (CONFIG_AUDIO_FRAME_DURATION_US == 7500)
#define FRAME_SIZE_BYTES (CONFIG_AUDIO_SAMPLE_RATE_HZ * 15 / 2 * CONFIG_AUDIO_BIT_DEPTH_OCTETS * CONFIG_I2S_CH_NUM / 1000)
#else
#define FRAME_SIZE_BYTES (CONFIG_AUDIO_SAMPLE_RATE_HZ * 10 * CONFIG_AUDIO_BIT_DEPTH_OCTETS * CONFIG_I2S_CH_NUM / 1000)
#endif

static struct sw_codec_config sw_codec_cfg;
static bool audio_codec_started;

static void _encoder_work_submit(struct k_timer * _unused);
static void _encoder(struct k_work * __unused);
static void _button_msgq_reciever_thread(void* _a, void* _b, void* _c);

struct k_timer _encoder_timer;
K_TIMER_DEFINE(_encoder_timer, _encoder_work_submit, NULL);

K_THREAD_STACK_DEFINE(_encoder_stack_area, CONFIG_ENCODER_STACK_SIZE);
struct k_work_q _encoder_work_queue;

K_WORK_DEFINE(_encoder_work, _encoder);


#define BUTTON_MSGQ_RX_STACK_SIZE 700
#define BUTTON_MSGQ_RX_PRIORITY 5

K_THREAD_STACK_DEFINE(_button_msgq_rx_stack_area, BUTTON_MSGQ_RX_STACK_SIZE);
struct k_thread _button_msgq_rx_thread_data;

void audio_generate_init(void) {
	
	synthesizer_init();

	k_work_queue_init(&_encoder_work_queue);
	k_work_queue_start(&_encoder_work_queue, _encoder_stack_area,
                   K_THREAD_STACK_SIZEOF(_encoder_stack_area), K_PRIO_PREEMPT(CONFIG_ENCODER_THREAD_PRIO),
                   NULL);

	(void)k_thread_create(&_button_msgq_rx_thread_data, _button_msgq_rx_stack_area,
                                 K_THREAD_STACK_SIZEOF(_button_msgq_rx_stack_area),
                                 _button_msgq_reciever_thread,
                                 NULL, NULL, NULL,
                                 BUTTON_MSGQ_RX_PRIORITY, 0, K_NO_WAIT);
}

void audio_generate_start(void) {
    int ret;


	if (IS_ENABLED(CONFIG_SW_CODEC_SBC)) {
		sw_codec_cfg.sw_codec = SW_CODEC_SBC;
	} else if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.sw_codec = SW_CODEC_LC3;
	}

#if (CONFIG_STREAM_BIDIRECTIONAL)
	sw_codec_cfg.decoder.enabled = true;
	sw_codec_cfg.decoder.channel_mode = SW_CODEC_MONO;
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

	if (IS_ENABLED(CONFIG_SW_CODEC_SBC)) {
		sw_codec_cfg.encoder.bitrate = SBC_MONO_BITRATE;
	} else if (IS_ENABLED(CONFIG_SW_CODEC_LC3)) {
		sw_codec_cfg.encoder.bitrate = CONFIG_LC3_MONO_BITRATE;
	}

#if (CONFIG_TRANSPORT_CIS)
	sw_codec_cfg.encoder.channel_mode = SW_CODEC_STEREO;
#else
	sw_codec_cfg.encoder.channel_mode = SW_CODEC_MONO;
#endif /* (CONFIG_TRANSPORT_CIS) */
	sw_codec_cfg.encoder.enabled = true;

	ret = sw_codec_init(sw_codec_cfg);
	ERR_CHK_MSG(ret, "Failed to set up codec");

	sw_codec_cfg.initialized = true;

	k_timer_start(&_encoder_timer, K_NO_WAIT, K_USEC(CONFIG_AUDIO_FRAME_DURATION_US));
		
	audio_codec_started = true;    
}

void audio_generate_stop(void) {
    int ret;

    if (!audio_codec_started) {
        LOG_WRN("Audio codec not started");
        return;
    }

    audio_codec_started = false;

    if (!sw_codec_cfg.initialized) {
        LOG_WRN("Codec already unitialized");
		return;
    }

    LOG_DBG("Stopping codec");
	/* Aborting encoder thread before uninitializing */
	if (sw_codec_cfg.encoder.enabled) {
		k_timer_stop(&_encoder_timer);
	}

    ret = sw_codec_uninit(sw_codec_cfg);
	ERR_CHK_MSG(ret, "Failed to uninit codec");
	sw_codec_cfg.initialized = false;
}

static void _encoder_work_submit(struct k_timer * _unused) {

	k_work_submit_to_queue(&_encoder_work_queue, &_encoder_work);
}

static void _encoder(struct k_work * _unused) {
	// TODO: stop thread when not in use -> cooperative thread
	if (sw_codec_cfg.encoder.enabled) {
		char pcm_raw_data[FRAME_SIZE_BYTES];
		memset(pcm_raw_data, 0, FRAME_SIZE_BYTES * sizeof pcm_raw_data[0]);

		/* audio proccessing here */
		(void)synthesizer_process(pcm_raw_data, FRAME_SIZE_BYTES / CONFIG_AUDIO_BIT_DEPTH_OCTETS);

		int ret;
		size_t encoded_data_size = 0;
		static uint8_t *encoded_data;
		ret = sw_codec_encode(pcm_raw_data, FRAME_SIZE_BYTES, &encoded_data,
						&encoded_data_size);

		ERR_CHK_MSG(ret, "Encode failed");

		/* Send encoded data over IPM */
		stream_control_encoded_data_send(encoded_data, encoded_data_size);
	}
}


static void _button_msgq_reciever_thread(void* _a, void* _b, void* _c) {
	(void)_a;
	(void)_b;
	(void)_c;

	while (1) {
		struct button_event event;
		button_event_get(&event, K_FOREVER);

		synthesizer_key_event(&event);
	}
}
