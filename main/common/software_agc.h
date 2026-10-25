#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SOFTWARE_AGC_PROBE_SAMPLES 512u
#define SOFTWARE_AGC_MAX_ROUNDS 6u
#define SOFTWARE_AGC_TARGET_DBFS (-12)
#define SOFTWARE_AGC_TARGET_PEAK 129u /* 10-bit IQ full scale is 512; 129 is -11.97 dBFS. */
#define SOFTWARE_AGC_RELEASE_DBFS (-18)
#define SOFTWARE_AGC_RELEASE_PEAK_MAX 64u /* 64/512 is -18.06 dBFS. */
#define SOFTWARE_AGC_ATTACK_DBFS (-6)
#define SOFTWARE_AGC_ATTACK_PEAK_MIN 257u /* 257/512 is -5.99 dBFS. */
#define SOFTWARE_AGC_RELEASE_CONFIRMATIONS 5u

static inline unsigned software_agc_peak(const uint32_t *iq, unsigned count) {
    unsigned peak = 0;
    for (unsigned j = 0; j < count; j++) {
        int32_t i = (int32_t)(iq[j] << 22) >> 22;
        int32_t q = (int32_t)(iq[j] << 12) >> 22;
        unsigned abs_i = (unsigned)(i < 0 ? -i : i);
        unsigned abs_q = (unsigned)(q < 0 ? -q : q);
        if (abs_i > peak) peak = abs_i;
        if (abs_q > peak) peak = abs_q;
    }
    return peak;
}

/* Control the largest raw I/Q component around -12 dBFS.
 * Raise gain below the -18 dBFS release threshold, and lower it above the
 * -6 dBFS attack threshold. Change one PHY index per update. */
static inline unsigned software_agc_next_gain(unsigned peak, unsigned current, unsigned minimum,
                                              unsigned maximum, unsigned *release_streak) {
    unsigned next = current;
    if (peak >= SOFTWARE_AGC_ATTACK_PEAK_MIN) {
        *release_streak = 0;
        next = current > minimum ? current - 1 : minimum;
    } else if (peak <= SOFTWARE_AGC_RELEASE_PEAK_MAX) {
        if (*release_streak < SOFTWARE_AGC_RELEASE_CONFIRMATIONS) (*release_streak)++;
        /* Require five consecutive low readings before slowly raising gain. */
        if (*release_streak >= SOFTWARE_AGC_RELEASE_CONFIRMATIONS && current < maximum) {
            next = current + 1;
            *release_streak = 0;
        }
    } else {
        *release_streak = 0;
    }
    if (next > maximum) next = maximum;
    return next;
}

/* Fast attack and slower release for raw IQ peaks between display frames. */
static inline unsigned software_agc_smooth_peak(unsigned peak, uint32_t *average_q8, bool *ready) {
    uint32_t sample_q8 = peak << 8;
    if (!*ready) {
        *average_q8 = sample_q8;
        *ready = true;
    } else if (sample_q8 >= *average_q8) {
        *average_q8 += (sample_q8 - *average_q8) / 2;
    } else {
        *average_q8 -= (*average_q8 - sample_q8) / 8;
    }
    return (*average_q8 + 128) >> 8;
}
