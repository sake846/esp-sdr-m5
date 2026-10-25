/* Shared on-device spectrum protocol. Acquisition callbacks return normalized
 * IQ10 words (I in bits 0..9, Q in 10..19) in stable CPU-owned memory. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef bool (*spectrum_acquire_fn)(unsigned n, unsigned rate, const uint32_t **words, unsigned *elapsed_us);
bool spectrum_command(const char *line, unsigned frequency_mhz, spectrum_acquire_fn acquire);

bool spectrum_fft_init(void);
bool spectrum_fft_init_n(unsigned n);
/* One n-point local snapshot in natural FFT order, without serial output.
 * Call only between serial leases; it shares the protocol FFT workspace. */
bool spectrum_snapshot(unsigned rate, unsigned n, spectrum_acquire_fn acquire, uint8_t *bins);
/* Reuse the captured IQ buffer as FFT work memory; acquire() must return it.
 * This overwrites the captured samples after acquisition. */
bool spectrum_snapshot_inplace(unsigned rate, unsigned n, spectrum_acquire_fn acquire, uint8_t *bins,
                               int16_t *workspace);
/* Like spectrum_snapshot_inplace, and reports the largest raw 10-bit I/Q
 * sample magnitude before DC removal and windowing. */
bool spectrum_snapshot_inplace_peak(unsigned rate, unsigned n, spectrum_acquire_fn acquire, uint8_t *bins,
                                   int16_t *workspace, unsigned *peak);
/* Reset the local snapshot DC tracker after retuning or other RF-path changes. */
void spectrum_snapshot_reset_dc(void);

/* Snapshot and continuous backends run exclusively. Reuse their large buffers
 * on SRAM-constrained targets; acquiring this workspace invalidates the cached
 * snapshot window so a later transport switch rebuilds it. */
typedef struct {
    int16_t *fft, *window;
    float *power;
    uint8_t *frame;
} spectrum_workspace_t;
spectrum_workspace_t spectrum_workspace(void);
