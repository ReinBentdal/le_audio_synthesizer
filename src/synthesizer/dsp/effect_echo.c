#include "effect_echo.h"

#include <zephyr/kernel.h>
#include "dsp_instructions.h"

void effect_echo_init(struct effect_echo* this) {
    __ASSERT_NO_MSG(this != NULL);

    *this = (struct effect_echo){
        .samples = {0},
        .head_index = 0,
        .tail_index = 1,
        .feedback_gain = INT16_MAX / 2,
    };
}

bool effect_echo_process(struct effect_echo* this, int8_t* block, size_t block_size) {
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT_NO_MSG(block != NULL);

    /* if no feedback, the module will have no effect on the signal */
    if (this->feedback_gain == 0) {
        return true;
    }

    for (int i = 0; i < block_size; i++) {

        const int16_t input_sample = ((int16_t*)block)[i];
        const int16_t feedback_sample = ((int32_t)this->samples[this->tail_index] * (int32_t)this->feedback_gain) >> 15;

        const int16_t output_sample = saturate16((int32_t)input_sample + (int32_t)feedback_sample);

        this->samples[this->head_index] = output_sample;

        /* increment indexes */
        this->tail_index++;
        if (this->tail_index == EFFECT_ECHO_MAX_SAMPLES) {
            this->tail_index = 0;
        }

        this->head_index++;
        if (this->head_index == EFFECT_ECHO_MAX_SAMPLES) {
            this->head_index = 0;
        }

        ((int16_t*)block)[i] = output_sample;
        
    }

    return true;
}

void effect_echo_set_delay(struct effect_echo* this, uint32_t delay_samples) {
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT(delay_samples <= EFFECT_ECHO_MAX_SAMPLES, "tried setting delay out of range");

    if (this->head_index >= delay_samples) {
        this->tail_index = this->head_index - delay_samples;
    } else {
        this->tail_index = this->head_index + EFFECT_ECHO_MAX_SAMPLES - delay_samples;
    }
}

void effect_echo_set_feedback(struct effect_echo* this, int16_t magnitute) {
    __ASSERT_NO_MSG(this != NULL);

    this->feedback_gain = magnitute;
}