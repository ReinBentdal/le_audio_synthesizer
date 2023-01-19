#include "filter_allpass.h"

#include <zephyr/kernel.h>
#include "dsp_instructions.h"
#include "integer_math.h"

#define GAIN2(g) (INT16_MAX-FIXED_MULTIPLY(g, g))

void filter_allpass_init(struct filter_allpass* this, fixed16* buffer, size_t buffer_size) {
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT_NO_MSG(buffer != NULL);

    const int16_t gain = FLOAT_TO_FIXED16(0.6);

    *this = (struct filter_allpass) {
        .buffer = buffer,
        .buffer_size = buffer_size,
        .head_index = 0,
        .tail_index = 0,
        .gain = gain,
        .gain2 = GAIN2(gain),
    };
}

void filter_allpass_set_gain(struct filter_allpass* this, fixed16 gain) {
    __ASSERT_NO_MSG(this != NULL);

    this->gain = gain;
    this->gain2 = GAIN2(gain);
}

void filter_allpass_set_delay(struct filter_allpass* this, uint32_t delay_ms) {
    __ASSERT_NO_MSG(this != NULL);

    const uint32_t delay_samples = (CONFIG_AUDIO_SAMPLE_RATE_HZ / 1000) * delay_ms;

    __ASSERT(delay_samples <= this->buffer_size, "tried setting delay out of range");

    if (this->head_index >= delay_samples) {
        this->tail_index = this->head_index - delay_samples;
    } else {
        this->tail_index = this->head_index + this->buffer_size - delay_samples;
    }  
}

bool filter_allpass_process(struct filter_allpass* this, fixed16* block, size_t block_size) {
    __ASSERT_NO_MSG(this != NULL);


    __ASSERT_NO_MSG(this != NULL);
    __ASSERT_NO_MSG(block != NULL);

    for (int i = 0; i < block_size; i++) {

        const fixed16 input_sample = block[i];
        const fixed16 input_forward_sample = FIXED_MULTIPLY(input_sample, -this->gain);

        const fixed16 delayed_sample = this->buffer[this->tail_index];
        const fixed16 feedback_sample = FIXED_MULTIPLY(delayed_sample, this->gain);

        const fixed16 output_sample = FIXED_ADD_SATURATE(input_forward_sample, FIXED_ADD(delayed_sample, this->gain2));

        this->buffer[this->head_index] = input_sample + feedback_sample; // should not be possible to overflow

        /* increment indexes */
        this->tail_index++;
        if (this->tail_index == this->buffer_size) {
            this->tail_index = 0;
        }

        this->head_index++;
        if (this->head_index == this->buffer_size) {
            this->head_index = 0;
        }

        block[i] = output_sample;
        
    }

    return true;    
}
