/* Shared on-device spectrum protocol. Acquisition callbacks return normalized
 * IQ10 words (I in bits 0..9, Q in 10..19) in stable CPU-owned memory. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef bool (*spectrum_acquire_fn)(unsigned n, unsigned rate, const uint32_t **words, unsigned *elapsed_us);
bool spectrum_command(const char *line, unsigned frequency_mhz, spectrum_acquire_fn acquire);

bool spectrum_fft_init(void);
/* One 256-bin local snapshot in natural FFT order, without serial output.
 * Call only between serial leases; it shares the protocol FFT workspace. */
bool spectrum_snapshot(unsigned rate, spectrum_acquire_fn acquire, uint8_t bins[256]);
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
