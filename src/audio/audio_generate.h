#ifndef _AUDIO_GENERATE_H_
#define _AUDIO_GENERATE_H_

#include <stdint.h>

void audio_generate_init(void);

/**
 * @brief Initialize and start audio on gateway
 */
void audio_generate_start(void);

/**
 * @brief Stop all activities related to audio
 */
void audio_generate_stop(void);

#endif /* _AUDIO_CODEC_H_ */