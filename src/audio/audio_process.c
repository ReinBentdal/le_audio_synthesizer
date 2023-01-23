#include "audio_process.h"

#include <zephyr/kernel.h>
#include "sw_codec.h"
#include "macros_common.h"
#include "stream_control.h"
#include "button.h"
#include "synthesizer.h"
#include "tick_provider.h"
#include "integer_math.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(audio_process, CONFIG_LOG_AUDIO_PROCESS_LEVEL);

static struct sw_codec_config _sw_codec_config;
static bool _audio_codec_started;

static void _audio_process_work_submit(struct k_timer * _unused);
static void _audio_process(struct k_work * _unused);

K_TIMER_DEFINE(_encoder_timer, _audio_process_work_submit, NULL);

K_THREAD_STACK_DEFINE(_encoder_stack_area, CONFIG_ENCODER_STACK_SIZE);
struct k_work_q _encoder_work_queue;
K_WORK_DEFINE(_encoder_work, _audio_process);

static struct tick_provider_subscriber _syntheiziser_tick_provider;

void audio_process_init(void) {
	
	LOG_DBG("synthesizer init");
	synthesizer_init();

	LOG_DBG("tick provider init");
	tick_provider_init();
	
	LOG_DBG("tick provider initial configuration");
	tick_provider_set_bpm(128);
	tick_provider_subscribe(&_syntheiziser_tick_provider, synthesizer_tick);

	/* audio blocks processed through a queue */
	// TODO: since the interval is the same as ble connection interval should it instead be directly syncronized with this interval. For example by radio notify interrupt.
	k_work_queue_init(&_encoder_work_queue);
	k_work_queue_start(&_encoder_work_queue, _encoder_stack_area, K_THREAD_STACK_SIZEOF(_encoder_stack_area), K_PRIO_PREEMPT(CONFIG_ENCODER_THREAD_PRIO), NULL);
}

void audio_process_start(void) {
    int ret;

	_sw_codec_config.sw_codec = SW_CODEC_LC3;
#if (CONFIG_STREAM_BIDIRECTIONAL)
	sw_codec_cfg.decoder.enabled = true;
	sw_codec_cfg.decoder.channel_mode = SW_CODEC_MONO;
#endif /* (CONFIG_STREAM_BIDIRECTIONAL) */

	_sw_codec_config.encoder.bitrate = CONFIG_LC3_MONO_BITRATE;

#if (CONFIG_TRANSPORT_CIS)
	_sw_codec_config.encoder.channel_mode = SW_CODEC_STEREO;
#else
	_sw_codec_config.encoder.channel_mode = SW_CODEC_MONO;
#endif /* (CONFIG_TRANSPORT_CIS) */
	_sw_codec_config.encoder.enabled = true;

	ret = sw_codec_init(_sw_codec_config);
	ERR_CHK_MSG(ret, "Failed to set up codec");

	_sw_codec_config.initialized = true;

	k_timer_start(&_encoder_timer, K_NO_WAIT, K_USEC(CONFIG_AUDIO_FRAME_DURATION_US));
		
	_audio_codec_started = true;    
}

void audio_process_stop(void) {
    int ret;

    if (!_audio_codec_started) {
        LOG_WRN("Audio codec not started");
        return;
    }

    _audio_codec_started = false;

    if (!_sw_codec_config.initialized) {
        LOG_WRN("Codec already unitialized");
		return;
    }

    LOG_DBG("Stopping codec");
	/* Aborting encoder thread before uninitializing */
	if (_sw_codec_config.encoder.enabled) {
		k_timer_stop(&_encoder_timer);
	}

    ret = sw_codec_uninit(_sw_codec_config);
	ERR_CHK_MSG(ret, "Failed to uninit codec");
	_sw_codec_config.initialized = false;
}


static void _audio_process_work_submit(struct k_timer * _unused) {
	k_work_submit_to_queue(&_encoder_work_queue, &_encoder_work);
}

static void _audio_process(struct k_work * _unused) {
	if (_sw_codec_config.encoder.enabled) {
		
		static fixed16 _audio_buf[AUDIO_BLOCK_SIZE];
		memset(_audio_buf, 0, AUDIO_BLOCK_SIZE * sizeof _audio_buf[0]);

		/* audio proccessing here */
		const bool did_process = synthesizer_process(_audio_buf, AUDIO_BLOCK_SIZE);
		(void)did_process;

		size_t encoded_data_size = 0;
		static uint8_t *encoded_data;
		int ret = sw_codec_encode(_audio_buf, FRAME_SIZE_BYTES, &encoded_data, &encoded_data_size);

		ERR_CHK_MSG(ret, "Encode failed");

		/* Send encoded data over IPM */
		stream_control_encoded_data_send(encoded_data, encoded_data_size);

		/* increment audio time. This might take time, because it also might call callbacks. Thus done after the block of audio is processed */
		tick_provider_increment();
	}
}
