#include "effect_echo.h"

#include <zephyr/kernel.h>
#include <string.h>

#include "dsp_instructions.h"
#include "integer_math.h"

void effect_echo_init(struct effect_echo* this, fixed16* buffer, size_t buffer_size) {
    __ASSERT_NO_MSG(this != NULL);

    /* initialize buffer to only zeros */
    memset(buffer, 0, buffer_size*sizeof(buffer[0]));

    *this = (struct effect_echo){
        .buffer = buffer,
        .buffer_size = buffer_size,
        .head_index = 0,
        .tail_index = 0,
        .feedback_gain = FLOAT_TO_FIXED16(0.5),
    };
}

bool effect_echo_process(struct effect_echo* this, fixed16* block, size_t block_size) {
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT_NO_MSG(block != NULL);

    /* if no feedback, the module will have no effect on the signal */
    if (this->feedback_gain == 0) {
        return true;
    }

    for (int i = 0; i < block_size; i++) {

        const fixed16 feedback_sample = FIXED_MULTIPLY(this->buffer[this->tail_index], this->feedback_gain);

        const fixed16 output_sample = FIXED_ADD_SATURATE(block[i], feedback_sample);

        this->buffer[this->head_index] = output_sample;

        block[i] = output_sample;

        /* increment indexes */
        this->tail_index++;
        if (this->tail_index == this->buffer_size) {
            this->tail_index = 0;
        }

        this->head_index++;
        if (this->head_index == this->buffer_size) {
            this->head_index = 0;
        }
    }

    return true;
}

void effect_echo_set_delay(struct effect_echo* this, uint32_t delay_ms) {
    __ASSERT_NO_MSG(this != NULL);

    const uint32_t delay_samples = CONFIG_AUDIO_SAMPLE_RATE_HZ * delay_ms / 1000;

    __ASSERT(delay_samples > 0 && delay_samples <= this->buffer_size, "tried setting delay out of range");

    if (this->head_index >= delay_samples) {
        this->tail_index = this->head_index - delay_samples;
    } else {
        this->tail_index = this->head_index + this->buffer_size - delay_samples;
    }
}

void effect_echo_set_feedback(struct effect_echo* this, fixed16 magnitute) {
    __ASSERT_NO_MSG(this != NULL);

    this->feedback_gain = magnitute;
}