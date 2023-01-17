/**
 * @file integer_math.h
 * @author Rein Gundersen Bentdal
 * @brief inline functions to make integer dsp easier
 * @version 0.1
 * @date 2023-01-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _INTEGER_MATH_H_
#define _INTEGER_MATH_H_

#include <stdint.h>

#include "dsp_instructions.h"

// assumes we are mainly interrested in 16-bit dsp with fixed16 point range of [-1, 1] and [0, 1]
typedef int16_t fixed16;
typedef uint16_t ufixed16;

// returns the equivalent integer value from the float range -1 to 1
static inline fixed16 FLOAT_TO_FIXED16(float val) __attribute__((always_inline, unused));
static inline fixed16 FLOAT_TO_FIXED16(float val) {
    return (fixed16)(val*INT16_MAX);
}

// returns the equivalent integer value from the float range -1 to 1
static inline fixed16 FLOAT_TO_UFIXED16(float val) __attribute__((always_inline, unused));
static inline fixed16 FLOAT_TO_UFIXED16(float val) {
    return (fixed16)(val*UINT16_MAX);
}

static inline float FIXED16_TO_FLOAT(fixed16 val) __attribute__((always_inline, unused)); 
static inline float FIXED16_TO_FLOAT(fixed16 val) {
    return 1.0f*val/INT16_MAX;
}

static inline float UFIXED16_TO_FLOAT(ufixed16 val) __attribute__((always_inline, unused)); 
static inline float UFIXED16_TO_FLOAT(ufixed16 val) {
    return 1.0f*val/UINT16_MAX;
}

static inline fixed16 FIXED_MULTIPLY(fixed16 a, fixed16 b) __attribute__((always_inline, unused));
static inline fixed16 FIXED_MULTIPLY(fixed16 a, fixed16 b) {
    return ((int32_t)a * (int32_t)b) >> 15;
}

static inline fixed16 UFIXED_MULTIPLY(fixed16 a, ufixed16 b) __attribute__((always_inline, unused));
static inline fixed16 UFIXED_MULTIPLY(fixed16 a, ufixed16 b) {
    return ((int32_t)a * (uint32_t)b) >> 16;
}

static inline fixed16 FIXED_ADD(fixed16 a, fixed16 b) __attribute__((always_inline, unused));
static inline fixed16 FIXED_ADD(fixed16 a, fixed16 b) {
    return a + b;
}

static inline fixed16 FIXED_ADD_SATURATE(fixed16 a, fixed16 b) __attribute__((always_inline, unused));
static inline fixed16 FIXED_ADD_SATURATE(fixed16 a, fixed16 b) {
    return saturate16((int32_t)a + (int32_t)b);
}

static inline fixed16 FIXED_INTERPOLATE(fixed16 a, fixed16 b, ufixed16 pos) __attribute__((always_inline, unused));
static inline fixed16 FIXED_INTERPOLATE(fixed16 a, fixed16 b, ufixed16 pos) {
    return ((b * pos) + (a * (UINT16_MAX + 1 - pos))) >> 16;
}

static inline fixed16 FIXED_INTERPOLATE_AND_SCALE(fixed16 a, fixed16 b, ufixed16 pos, fixed16 magnitude) __attribute__((always_inline, unused));
static inline fixed16 FIXED_INTERPOLATE_AND_SCALE(fixed16 a, fixed16 b, ufixed16 pos, fixed16 magnitude) {
    return multiply_32x32_rshift32((b * pos) + (a * (UINT16_MAX + 1 - pos)), (int32_t)magnitude);
}

#endif