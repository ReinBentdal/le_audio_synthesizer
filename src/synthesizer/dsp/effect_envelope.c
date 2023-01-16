#include "effect_envelope.h"

#include <zephyr/kernel.h>
#include <arm_math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dsp, CONFIG_LOG_DSP_LEVEL);

#define SAMPLES_PER_MSEC (CONFIG_AUDIO_SAMPLE_RATE_HZ / 1000.f)

/* calculates the envelope magnitude to apply at a spesific position */
static int16_t _calculate_envelope_magnitude(struct effect_envelope *this, float position);
static inline void _set_envelope_state(struct effect_envelope *this, enum envelope_state state);

void effect_envelope_init(struct effect_envelope *this)
{
    __ASSERT_NO_MSG(this != NULL);

    *this = (struct effect_envelope){
        .phase_accumulator = 0,
        .phase_increment = 0,
        .rising_curve = 0.1,
        .falling_curve = 0.1,
        .fade_out_attenuation = 0.05,
        .floor_level = 0.0,
        .duty_cycle = 0.05,
        .mode = ENVELOPE_MODE_LOOP,
        .state = ENVELOPE_STATE_SILENT,
        .new_state = false,
        .magnitude_next = 0,
    };
}

void effect_envelope_start(struct effect_envelope *this)
{
    __ASSERT_NO_MSG(this != NULL);
    _set_envelope_state(this, ENVELOPE_STATE_LOOP);
}

void effect_envelope_end(struct effect_envelope *this)
{
    __ASSERT_NO_MSG(this != NULL);
    _set_envelope_state(this, ENVELOPE_STATE_FADE_OUT);
}

void effect_envelope_set_periode(struct effect_envelope *this, float ms)
{
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT(ms >= -CONFIG_AUDIO_SAMPLE_RATE_HZ / 2 && ms <= CONFIG_AUDIO_SAMPLE_RATE_HZ, "envelope periode out of range");

    this->phase_increment = 1000 * (2.f*INT32_MAX / CONFIG_AUDIO_SAMPLE_RATE_HZ) / ms;
}

void effect_envelope_set_duty_cycle(struct effect_envelope *this, float duty)
{
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT(duty >= 0 && duty <= 1, "envelope duty out of range");

    this->duty_cycle = duty;
}

void effect_envelope_set_floor(struct effect_envelope *this, float floor)
{
    __ASSERT_NO_MSG(this != NULL);
    __ASSERT(floor >= 0 && floor <= 1, "envelope floor out of range");

    this->floor_level = floor;
}

void effect_envelope_set_rising_curve(struct effect_envelope *this, float curve)
{
    __ASSERT_NO_MSG(this != NULL);

    const float CURVE_MIN = 0.00001f;

    /* curve of 1 results in 0 division */
    if (curve == 1.f)
    {
        curve = 0.99f;
    }

    /* curve be instand jump. Arbitrary, close to 0, min val */
    else if (curve <= CURVE_MIN)
    {
        curve = CURVE_MIN;
    }

    this->rising_curve = curve;
}

void effect_envelope_set_falling_curve(struct effect_envelope *this, float curve)
{
    __ASSERT_NO_MSG(this != NULL);

    const float CURVE_MIN = 0.0001f;

    /* curve of 1 results in 0 division */
    if (curve == 1.f)
    {
        curve = 0.99f;
    }

    /* curve be instand jump. Arbitrary, close to 0, min val */
    else if (curve <= CURVE_MIN)
    {
        curve = CURVE_MIN;
    }

    this->falling_curve = curve;
}

void effect_envelope_set_fade_out_attenuation(struct effect_envelope *this, float a)
{
    __ASSERT_NO_MSG(this != NULL);

    /* 1 results in complete attenuation in one block */
    if (a > 1)
        a = 1;

    /* cannot be 0 because it would result in never decaying */
    else if (a < 0.001)
        a = 0.001;

    this->fade_out_attenuation = a;
}

void effect_envelope_set_mode(struct effect_envelope* this, enum envelope_mode mode) {
    __ASSERT_NO_MSG(this != NULL);

    this->mode = mode;
}

bool effect_envelope_is_active(struct effect_envelope *this)
{
    __ASSERT_NO_MSG(this != NULL);
    return this->state != ENVELOPE_STATE_SILENT;
}

bool effect_envelope_process(struct effect_envelope *this, int8_t *block, size_t block_size)
{
    __ASSERT_NO_MSG(this != NULL);

    switch (this->state)
    {
    case ENVELOPE_STATE_LOOP:
    {
        if (this->new_state)
        {
            this->phase_accumulator = 0;
            this->new_state = false;
        }

        const int32_t start_magnitude = this->magnitude_next;

        const uint32_t accumulator_upper = this->phase_accumulator + this->phase_increment * block_size;

        int32_t end_magnitude;
        if (this->phase_accumulator < accumulator_upper || this->mode == ENVELOPE_MODE_LOOP)
        {
            const float end_envelope_position = (float)(accumulator_upper) / UINT32_MAX;
            end_magnitude = (int32_t)_calculate_envelope_magnitude(this, end_envelope_position);
        }

        /* if ONE_SHOT mode, interpolate last section to floor_level */
        else
        {
            end_magnitude = INT16_MAX * this->floor_level;

            if (this->mode == ENVELOPE_MODE_ONE_SHOT_HOLD)
            {
                _set_envelope_state(this, ENVELOPE_STATE_HOLD);
            }
            else if (this->mode == ENVELOPE_MODE_ONE_SHOT)
            {
                _set_envelope_state(this, ENVELOPE_STATE_FADE_OUT);
            }
        }

        for (int i = 0; i < block_size; i++)
        {
            const uint32_t scale = i * UINT16_MAX / block_size;

            const int32_t end_scale = end_magnitude * scale;
            const int32_t start_scale = start_magnitude * (0x10000 - scale);
            const int32_t magnitude = (start_scale + end_scale) >> 16;

            int32_t sample_L = ((int16_t*)block)[i];
            sample_L = (sample_L * magnitude) >> 15;
            ((int16_t *)block)[i] = sample_L;
        }
        this->phase_accumulator = accumulator_upper;
        this->magnitude_next = end_magnitude;
        break;
    }
    case ENVELOPE_STATE_FADE_OUT:
    {
        const int32_t start = this->magnitude_next;
        const int32_t end = start * (1 - this->fade_out_attenuation);

        for (int i = 0; i < block_size; i++)
        {
            const uint32_t scale = i * UINT16_MAX / block_size;

            const int32_t end_scale = end * scale;
            const int32_t start_scale = start * (0x10000 - scale);
            const int32_t magnitude = (start_scale + end_scale) >> 16;

            int32_t sample_L = ((int16_t*)block)[i];
            sample_L = (sample_L * magnitude) >> 15;
            ((int16_t *)block)[i] = sample_L;
        }

        if (end > FADE_OUT_THRESHOLD)
        {
            this->magnitude_next = end;
        }
        else
        {
            this->magnitude_next = 0;
            _set_envelope_state(this, ENVELOPE_STATE_SILENT);
        }

        break;
    }
    case ENVELOPE_STATE_HOLD:
    {
        const int32_t magnitude = this->magnitude_next;
        for (int i = 0; i < block_size; i++)
        {
            int32_t sample_L = ((int16_t*)block)[i];
            sample_L = (sample_L * magnitude) >> 15;
            ((int16_t *)block)[i] = sample_L;
        }
        break;
    }
    case ENVELOPE_STATE_SILENT:
        /* assumes samples wont be used if false is returned */
        return false;
    }
    return true;
}

static int16_t _calculate_envelope_magnitude(struct effect_envelope *this, float position)
{
    __ASSERT_NO_MSG(this != NULL);

    /* rising envelope curve */
    if (position <= this->duty_cycle)
    {
        if (position == 0)
        {
            return INT16_MAX * this->floor_level;
        }

        const float l = this->floor_level;
        const float d = this->duty_cycle;
        const float c = this->rising_curve;
        return INT16_MAX * (l + (1 - powf(c, position / d)) * (1 - l) / (1 - c));
    }

    /* falling envelope curve */
    else
    {
        if (position == 1)
        {
            return INT16_MAX * this->floor_level;
        }

        const float l = this->floor_level;
        const float d = this->duty_cycle;
        const float c = this->falling_curve;
        return INT16_MAX * (l + (powf(c, (position - d) / (1 - d)) - c) * (1 - l) / (1 - c));
    }
}

static inline void _set_envelope_state(struct effect_envelope *this, enum envelope_state state)
{
    __ASSERT_NO_MSG(this != NULL);

    this->state = state;
    this->new_state = true;
}