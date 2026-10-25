# M5Stack performance changes

The changes preserve FFT limits, averaging, the serial protocol and displayed
pixels. Capture sentinels, RF calibration and peripheral pin protection remain
part of the receive path.

## Memory

Basic, Core2 and StickC Plus2 now run local FFTs directly in the completed RF
capture buffer, as Cardputer already did. The Hann/twiddle tables grow to the
selected size; the separate FFT, power and frame buffers are allocated when used.
The portable serial spectrum workspace needs at most 2048 points. Cardputer's
continuous ring backend retains its separate workspace. Non-destructive snapshots
still support the full local FFT range by allocating their own buffer.

| Measurement | Before | After |
| --- | ---: | ---: |
| `spectrum.c` BSS, Basic/Core2/Plus2 | 75,838 B | 65 B |
| FFT table heap payload for a default 1024-point local snapshot | 0 B (static tables above) | 4,096 B |
| Approximate default RAM reclaimed on these three boards | — | 70 KiB |
| `spectrum_fft_init()` FFT buffer, Cardputer/ADV | 16,384 B | 8,192 B |

BSS and code sizes were measured by compiling the affected sources with the
existing five ESP-IDF board configurations and Xtensa GCC 16.1.0. The RAM estimate
excludes allocator metadata. Allocated buffers retain their largest used size;
changing to a smaller FFT does not repeatedly free/reallocate memory. A larger
bit-reversed twiddle table also serves smaller FFTs without regeneration.

The ESP32 source objects gain approximately 0.6–0.7 KiB of code for deferred
allocation; the Cardputer/ADV source objects become slightly smaller. These are
object sizes, not a claim about the complete firmware image.

## Rendering

LCD column bounds are set once per frame. Each row still receives its own page
address and memory-write command. Empty rows/margins use `memset`; waterfall row
selection and grid-row decisions happen outside the pixel loop. Basic's CPU
label is formatted once per frame.

| Per frame | Before | After |
| --- | ---: | ---: |
| SPI transactions, Basic/Core2/Plus2 | 1,440 | 962 |
| SPI transactions, Cardputer/ADV | 810 | 542 |
| String formatting calls, Basic/Core2 | 244 | 5 |

In a host benchmark of 1000 Basic frames, the median of three runs fell from
162 ms to 83 ms (about 49%). Both versions produced pixel hash `1508956624`.
The workload uses deterministic 8192-bin input and cycles all spans, frequencies
and CPU labels. It uses MSVC `/O2` and mocked SPI; it measures host rendering
work, not device frame rate. LCD payload size and SPI clock are unchanged.

Run the current benchmark from a compiler-enabled shell:

```sh
python tests/benchmark_m5stack_display.py
```

Regression tests check numerical FFT results, in-place acquisition, allocation
failure/retry, the default 4 KiB table budget, the 2048-point protocol workspace,
one column-window update per frame, geometry and waterfall history. All 24
related tests and the five firmware builds/partition size checks passed. See
[the test instructions](../tests/README.md). Physical RF and LCD behavior still
needs validation on the boards.
