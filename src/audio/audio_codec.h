#ifndef _AUDIO_CODEC_H_
#define _AUDIO_CODEC_H_

#include <stdint.h>

void audio_codec_init(void);

/**
 * @brief Initialize and start audio on gateway
 */
void audio_codec_gateway_start(void);

/**
 * @brief Stop all activities related to audio
 */
void audio_codec_stop(void);

#endif /* _AUDIO_CODEC_H_ */