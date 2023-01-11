/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sw_codec.h"

#include <zephyr/kernel.h>
#include <errno.h>

#if (CONFIG_SW_CODEC_LC3)
#include "sw_codec_lc3.h"
#endif /* (CONFIG_SW_CODEC_LC3) */
#if (CONFIG_SW_CODEC_SBC)
#include "oi_codec_sbc.h"
#include "sbc_encoder.h"
#endif /* (CONFIG_SW_CODEC_SBC) */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sw_codec_select);

static struct sw_codec_config m_config;

static int _pcm_two_channel_split(void const *const input, size_t input_size, uint8_t pcm_bit_depth, void *output_left, void *output_right, size_t *output_size);
static bool _is_valid_bit_depth(uint8_t pcm_bit_depth);
static bool _is_valid_size(size_t size, uint8_t bytes_per_sample, uint8_t no_channels);

#if (CONFIG_SW_CODEC_SBC)
static bool sbc_first_frame_received;
static OI_CODEC_SBC_DECODER_CONTEXT context;
static SBC_ENC_PARAMS m_sbc_enc_params;
static OI_CODEC_SBC_CODEC_DATA_STEREO sw_codec_sbc_dec_data;

#define LAST_PCM_FRAME_START_IDX                                                                   \
	(PCM_NUM_BYTES_SBC_FRAME_MONO * (CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET - 1))

/* Since SBC remembers the previous frame when encoding, we need to force it to
 * remember the 'correct' last frame when encoding mono twice
 */
static void _prev_frame_sbc_flush(char *pcm_data);

#endif /* (CONFIG_SW_CODEC_SBC) */

int sw_codec_encode(void *pcm_data, size_t pcm_size, uint8_t **encoded_data, size_t *encoded_size)
{
	/* Temp storage for split stereo PCM signal */
	char pcm_data_mono[AUDIO_CH_NUM][PCM_NUM_BYTES_MONO] = { 0 };
	/* Make sure we have enough space for two frames (stereo) */
	static uint8_t m_encoded_data[ENC_MAX_FRAME_SIZE * AUDIO_CH_NUM];

	size_t pcm_block_size_mono;
	int ret;

	if (!m_config.encoder.enabled) {
		LOG_ERR("Encoder has not been initialized");
		return -ENXIO;
	}

	switch (m_config.sw_codec) {
	case SW_CODEC_LC3: {
#if (CONFIG_SW_CODEC_LC3)
		uint16_t encoded_bytes_written;

		/* Since LC3 is a single channel codec, we must split the
		 * stereo PCM stream
		 */
		ret = _pcm_two_channel_split(pcm_data, pcm_size, CONFIG_AUDIO_BIT_DEPTH_BITS,
					     pcm_data_mono[AUDIO_CH_L], pcm_data_mono[AUDIO_CH_R],
					     &pcm_block_size_mono);
		if (ret) {
			return ret;
		}

		switch (m_config.encoder.channel_mode) {
		case SW_CODEC_MONO: {
			ret = sw_codec_lc3_enc_run(pcm_data_mono[m_config.encoder.audio_ch],
						   pcm_block_size_mono, LC3_USE_BITRATE_FROM_INIT,
						   0, sizeof(m_encoded_data), m_encoded_data,
						   &encoded_bytes_written);
			if (ret) {
				return ret;
			}
			break;
		}
		case SW_CODEC_STEREO: {
			ret = sw_codec_lc3_enc_run(pcm_data_mono[AUDIO_CH_L], pcm_block_size_mono,
						   LC3_USE_BITRATE_FROM_INIT, AUDIO_CH_L,
						   sizeof(m_encoded_data), m_encoded_data,
						   &encoded_bytes_written);
			if (ret) {
				return ret;
			}

			ret = sw_codec_lc3_enc_run(pcm_data_mono[AUDIO_CH_R], pcm_block_size_mono,
						   LC3_USE_BITRATE_FROM_INIT, AUDIO_CH_R,
						   sizeof(m_encoded_data) - encoded_bytes_written,
						   m_encoded_data + encoded_bytes_written,
						   &encoded_bytes_written);
			if (ret) {
				return ret;
			}
			encoded_bytes_written += encoded_bytes_written;
			break;
		}
		default:
			LOG_ERR("Unsupported channel mode: %d", m_config.encoder.channel_mode);
			return -ENODEV;
		}

		*encoded_data = m_encoded_data;
		*encoded_size = encoded_bytes_written;

#endif /* (CONFIG_SW_CODEC_LC3) */
		break;
	}
	case SW_CODEC_SBC: {
#if (CONFIG_SW_CODEC_SBC)
		static uint8_t pcm_data_prev_frame[AUDIO_CH_NUM][PCM_NUM_BYTES_SBC_FRAME_MONO];

		ret = _pcm_two_channel_split(pcm_data, pcm_size, CONFIG_AUDIO_BIT_DEPTH_BITS,
					     pcm_data_mono[AUDIO_CH_L], pcm_data_mono[AUDIO_CH_R],
					     &pcm_block_size_mono);
		if (ret) {
			return ret;
		}

		switch (m_config.encoder.channel_mode) {
		case SW_CODEC_MONO: {
			m_sbc_enc_params.ps16PcmBuffer =
				(int16_t *)pcm_data_mono[m_config.encoder.audio_ch];
			m_sbc_enc_params.pu8Packet = m_encoded_data;
			m_sbc_enc_params.u8NumPacketToEncode = CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET;

			/* Encode PCM data to SBC */
			SBC_Encoder(&m_sbc_enc_params);
			*encoded_size = m_sbc_enc_params.u16PacketLength *
					CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET;
			*encoded_data = m_encoded_data;
			break;
		}
		case SW_CODEC_STEREO: {
			/* This SBC implementation only supports single instance.
			 * Since the same instance is used for both left
			 * and right channel, we need to swap out the last
			 * encoded frame with the correct channel before encoding.
			 * This leads to a 20% overhead, but without it, channels
			 * will mix leading to poor audio quality.
			 */
			_prev_frame_sbc_flush(pcm_data_prev_frame[AUDIO_CH_L]);

			/* Encode left channel */
			m_sbc_enc_params.ps16PcmBuffer = (int16_t *)pcm_data_mono[AUDIO_CH_L];
			m_sbc_enc_params.pu8Packet = m_encoded_data;
			m_sbc_enc_params.u8NumPacketToEncode = CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET;

			/* Encode PCM data to SBC */
			SBC_Encoder(&m_sbc_enc_params);

			*encoded_size = m_sbc_enc_params.u16PacketLength *
					CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET;

			_prev_frame_sbc_flush(pcm_data_prev_frame[AUDIO_CH_R]);

			/* Encode right channel */
			m_sbc_enc_params.ps16PcmBuffer = (int16_t *)pcm_data_mono[AUDIO_CH_R];
			m_sbc_enc_params.pu8Packet = &m_encoded_data[*encoded_size];
			m_sbc_enc_params.u8NumPacketToEncode = CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET;

			/* Encode PCM data to SBC */
			SBC_Encoder(&m_sbc_enc_params);

			*encoded_data = m_encoded_data;
			*encoded_size = m_sbc_enc_params.u16PacketLength *
					CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET * 2;

			/* Remember last frame */
			memcpy(pcm_data_prev_frame[AUDIO_CH_L],
			       &pcm_data_mono[AUDIO_CH_L][LAST_PCM_FRAME_START_IDX],
			       PCM_NUM_BYTES_SBC_FRAME_MONO);
			memcpy(pcm_data_prev_frame[AUDIO_CH_R],
			       &pcm_data_mono[AUDIO_CH_R][LAST_PCM_FRAME_START_IDX],
			       PCM_NUM_BYTES_SBC_FRAME_MONO);

			break;
		}
		default:
			LOG_ERR("Unsupported channel mode: %d", m_config.encoder.channel_mode);
			return -ENODEV;
		}
		break;
#endif /* (CONFIG_SW_CODEC_SBC) */
		LOG_ERR("Not supported");
		return -ENODEV;
	}
	default:
		LOG_ERR("Unsupported codec: %d", m_config.sw_codec);
		return -ENODEV;
	}

	return 0;
}

int sw_codec_uninit(struct sw_codec_config sw_codec_cfg)
{
	int ret;

	if (m_config.sw_codec != sw_codec_cfg.sw_codec) {
		LOG_ERR("Trying to uninit a codec that is not first initialized");
		return -ENODEV;
	}
	switch (m_config.sw_codec) {
	case SW_CODEC_LC3:
#if (CONFIG_SW_CODEC_LC3)
		if (sw_codec_cfg.encoder.enabled) {
			if (!m_config.encoder.enabled) {
				LOG_ERR("Trying to uninit encoder, it has not been initialized");
				return -EALREADY;
			}
			ret = sw_codec_lc3_enc_uninit_all();
			if (ret) {
				return ret;
			}
			m_config.encoder.enabled = false;
		}

		if (sw_codec_cfg.decoder.enabled) {
			if (!m_config.decoder.enabled) {
				LOG_WRN("Trying to uninit decoder, it has not been initialized");
				return -EALREADY;
			}
			ret = sw_codec_lc3_dec_uninit_all();
			if (ret) {
				return ret;
			}
			m_config.decoder.enabled = false;
		}
#endif /* (CONFIG_SW_CODEC_LC3) */
		break;
	case SW_CODEC_SBC:
#if (CONFIG_SW_CODEC_SBC)
		/* No clean up function for SBC codec */
		ret = 0;
		break;
#endif /* (CONFIG_SW_CODEC_SBC) */
		LOG_ERR("Not supported");
		return -ENODEV;
	default:
		LOG_ERR("Unsupported codec: %d", m_config.sw_codec);
		return false;
	}
	return 0;
}

int sw_codec_init(struct sw_codec_config sw_codec_cfg)
{
	switch (sw_codec_cfg.sw_codec) {
	case SW_CODEC_LC3: {
#if (CONFIG_SW_CODEC_LC3)
		int ret;

		if (m_config.sw_codec != SW_CODEC_LC3) {
			/* Check if LC3 is already initialized */
			ret = sw_codec_lc3_init(NULL, NULL, CONFIG_AUDIO_FRAME_DURATION_US);
			if (ret) {
				return ret;
			}
		}

		if (sw_codec_cfg.encoder.enabled) {
			if (m_config.encoder.enabled) {
				LOG_WRN("The LC3 encoder is already initialized");
				return -EALREADY;
			}
			uint16_t pcm_bytes_req_enc;

			ret = sw_codec_lc3_enc_init(
				CONFIG_AUDIO_SAMPLE_RATE_HZ, CONFIG_AUDIO_BIT_DEPTH_BITS,
				CONFIG_AUDIO_FRAME_DURATION_US, sw_codec_cfg.encoder.bitrate,
				sw_codec_cfg.encoder.channel_mode, &pcm_bytes_req_enc);
			if (ret) {
				return ret;
			}
		}

		if (sw_codec_cfg.decoder.enabled) {
			if (m_config.decoder.enabled) {
				LOG_WRN("The LC3 decoder is already initialized");
				return -EALREADY;
			}
			ret = sw_codec_lc3_dec_init(CONFIG_AUDIO_SAMPLE_RATE_HZ,
						    CONFIG_AUDIO_BIT_DEPTH_BITS,
						    CONFIG_AUDIO_FRAME_DURATION_US,
						    sw_codec_cfg.decoder.channel_mode);
			if (ret) {
				return ret;
			}
		}
		break;
#endif /* (CONFIG_SW_CODEC_LC3) */
		LOG_ERR("LC3 is not compiled in, please open menuconfig and select LC3");
		return -ENODEV;
	}
	case SW_CODEC_SBC: {
#if (CONFIG_SW_CODEC_SBC)
		m_sbc_enc_params.s16ChannelMode = CONFIG_SBC_CHANNEL_MODE_MONO;

		uint8_t sbc_pcm_stride;

		/* Since we encode mono+mono instead of stereo, numOfChannels will always be 1 */
		m_sbc_enc_params.s16NumOfChannels = 1;
		m_sbc_enc_params.s16SamplingFreq = SBC_sf48000;
		m_sbc_enc_params.s16NumOfBlocks = CONFIG_SBC_NO_OF_BLOCKS;
		m_sbc_enc_params.s16NumOfSubBands = CONFIG_SBC_NO_OF_SUBBANDS;
		m_sbc_enc_params.s16BitPool = CONFIG_SBC_BITPOOL;
		/* BitPool will be set in the driver by bitrate */

		m_sbc_enc_params.s16AllocationMethod = CONFIG_SBC_BIT_ALLOC_METHOD;
		m_sbc_enc_params.mSBCEnabled = 0;
		sbc_first_frame_received = false;
		SBC_Encoder_Init(&m_sbc_enc_params);

		/* Since only mono decode is supported when using SBC,
		 * pcm_stride will always be 1
		 */
		sbc_pcm_stride = 1;

		(void)OI_CODEC_SBC_DecoderReset(&context, sw_codec_sbc_dec_data.data,
						sizeof(sw_codec_sbc_dec_data), SBC_MAX_CHANNELS,
						sbc_pcm_stride, false);
		break;
#endif /* (CONFIG_SW_CODEC_SBC) */
		LOG_ERR("SBC is not compiled in, please open menuconfig and select SBC");
		return -ENODEV;
	}
	default:
		LOG_ERR("Unsupported codec: %d", sw_codec_cfg.sw_codec);
		return false;
	}

	m_config = sw_codec_cfg;
	return 0;
}

#if (CONFIG_SW_CODEC_SBC)
static void _prev_frame_sbc_flush(char *pcm_data)
{
	uint8_t last_frame_enc[ENC_MAX_FRAME_SIZE / CONFIG_SBC_NUM_FRAMES_PER_BLE_PACKET];

	/* Swap out last frame */
	m_sbc_enc_params.ps16PcmBuffer = (int16_t *)pcm_data;
	m_sbc_enc_params.pu8Packet = last_frame_enc;
	m_sbc_enc_params.u8NumPacketToEncode = 1;

	/* Don't flush data for first frame */
	if (sbc_first_frame_received) {
		SBC_Encoder(&m_sbc_enc_params);
	} else {
		sbc_first_frame_received = true;
	}
}
#endif

static int _pcm_two_channel_split(void const *const input, size_t input_size, uint8_t pcm_bit_depth, void *output_left, void *output_right, size_t *output_size)
{
	uint8_t bytes_per_sample = pcm_bit_depth / 8;

	if (!_is_valid_bit_depth(pcm_bit_depth) || !_is_valid_size(input_size, bytes_per_sample, 2)) {
		return -EINVAL;
	}

	char *pointer_input = (char *)input;
	char *pointer_output_left = (char *)output_left;
	char *pointer_output_right = (char *)output_right;

	for (uint32_t i = 0; i < input_size / bytes_per_sample; i += 2) {
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output_left++ = *pointer_input++;
		}
		for (uint8_t j = 0; j < bytes_per_sample; j++) {
			*pointer_output_right++ = *pointer_input++;
		}
	}

	*output_size = input_size / 2;
	return 0;
}

static bool _is_valid_bit_depth(uint8_t pcm_bit_depth)
{
	if (pcm_bit_depth != 16 && pcm_bit_depth != 24 && pcm_bit_depth != 32) {
		LOG_ERR("Invalid bit depth: %d", pcm_bit_depth);
		return false;
	}

	return true;
}

static bool _is_valid_size(size_t size, uint8_t bytes_per_sample, uint8_t no_channels)
{
	if (size % (bytes_per_sample * no_channels) != 0) {
		LOG_ERR("Size: %d is not dividable with number of bytes per sample x num channels",
			size);
		return false;
	}

	return true;
}