/* Integer power-to-wire conversion, shared by scalar spectrum backends.
 * Piecewise-linear log2 approximation has <0.06 dB error before 0.5 dB
 * quantization. It avoids soft-float logarithms in the capture deadline. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef int8_t spectrum_dbfs_t; /* Whole dBFS, rounded to 1 dB for the compact on-device history. */
#define SPECTRUM_DBFS_MIN (-84)
#define SPECTRUM_DBFS_MAX 43

typedef struct {
    int32_t q8;
    bool ready;
} spectrum_noise_floor_t;

static inline spectrum_dbfs_t spectrum_code_to_dbfs(uint8_t code) {
    int tenths = (int)code * 5 - 843; /* code / 2 - 84.3 dBFS */
    return (spectrum_dbfs_t)(tenths >= 0 ? (tenths + 5) / 10 : (tenths - 5) / 10);
}

/* Smooth the minimum in-band FFT-bin level with a 1/32 EWMA per frame. */
static inline spectrum_dbfs_t spectrum_noise_floor_update(spectrum_noise_floor_t *state,
                                                           spectrum_dbfs_t frame_minimum) {
    int32_t target = (int32_t)frame_minimum * 256;
    if (!state->ready) {
        state->q8 = target;
        state->ready = true;
    } else {
        state->q8 += (target - state->q8) / 32;
    }
    int32_t rounded = state->q8 >= 0 ? (state->q8 + 128) / 256 : (state->q8 - 128) / 256;
    return (spectrum_dbfs_t)rounded;
}

static inline uint8_t spectrum_dbfs_to_display_value(spectrum_dbfs_t dbfs, int lower_dbfs,
                                                     unsigned range_db) {
    if (!range_db || dbfs <= lower_dbfs) return 0;
    unsigned offset = (unsigned)(dbfs - lower_dbfs);
    if (offset >= range_db) return 255;
    return (uint8_t)((offset * 255 + range_db / 2) / range_db);
}

static inline uint32_t spectrum_complex_power(int16_t re, int16_t im) {
    /* Each product fits int32_t; the sum can be exactly 2^31. */
    return (uint32_t)((int32_t)re * re) + (uint32_t)((int32_t)im * im);
}
static inline uint8_t spectrum_log_power_code(unsigned exponent, uint32_t norm) {
    static const uint16_t log2_q12[17] = {0,    358,  696,  1016, 1319, 1607, 1882, 2144, 2396,
                                          2637, 2869, 3092, 3307, 3515, 3715, 3908, 4096};
    unsigned slot = (norm - 256u) >> 4, frac = norm & 15u;
    uint32_t logarithm =
        (exponent << 12) + log2_q12[slot] + ((log2_q12[slot + 1] - log2_q12[slot]) * frac + 8) / 16;
    unsigned code = (logarithm * 1541u + (1u << 19)) >> 20; /* 20 log10(power), rounded */
    return code > 255 ? 255 : code;
}
static inline uint8_t spectrum_power_code(uint32_t power) {
    if (!power) return 0;
    unsigned exponent = 31u - __builtin_clz(power);
    return spectrum_log_power_code(exponent, (power << (31u - exponent)) >> 23);
}
/* Averaged powers retain fractions: truncating them to integer power would
 * bias quiet bins down by several wire steps. Extract the float mantissa
 * directly, using the same approximation as the integer encoder. */
static inline uint8_t spectrum_mean_power_code(float power) {
    union {
        float f;
        uint32_t u;
    } value = {.f = power};
    /* Positive IEEE-754 encodings sort numerically. Also reject NaNs and
     * negative values without calling the software float comparator. */
    if (value.u < 0x3f800000u || value.u > 0x7f800000u) return 0;
    unsigned exponent = (value.u >> 23) - 127u;
    if (exponent >= 43) return 255;
    return spectrum_log_power_code(exponent, 256u + ((value.u & 0x7fffffu) >> 15));
}
