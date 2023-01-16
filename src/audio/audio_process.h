#ifndef _AUDIO_GENERATE_H_
#define _AUDIO_GENERATE_H_

#include <stdint.h>

#if (CONFIG_AUDIO_FRAME_DURATION_US == 7500)
#define FRAME_SIZE_BYTES (CONFIG_AUDIO_SAMPLE_RATE_HZ * 15 / 2 * CONFIG_AUDIO_BIT_DEPTH_OCTETS * CONFIG_I2S_CH_NUM / 1000)
#else
#define FRAME_SIZE_BYTES (CONFIG_AUDIO_SAMPLE_RATE_HZ * 10 * CONFIG_AUDIO_BIT_DEPTH_OCTETS * CONFIG_I2S_CH_NUM / 1000)
#endif

#define AUDIO_BLOCK_SIZE FRAME_SIZE_BYTES / CONFIG_AUDIO_BIT_DEPTH_OCTETS

void audio_process_init(void);

/**
 * @brief Initialize and start audio on gateway
 */
void audio_process_start(void);

/**
 * @brief Stop all activities related to audio
 */
void audio_process_stop(void);

#endif /* _AUDIO_CODEC_H_ */