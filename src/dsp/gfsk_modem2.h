#ifndef DSP_GFSK_MODEM2_H_
#define DSP_GFSK_MODEM2_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>
#include "../api.pb-c.h"

// GFSK modem built on liquid-dsp's continuous-phase FSK (cpfskmod) with a gaussian frequency pulse.
// It is a drop-in alternative to gfsk_modem.h: same settings, same soft-symbol output convention
// (int8, positive = bit 1, +-127 = +-deviation), so both can be compared on the same signals.
//
// Differences from gfsk_modem:
//  - modulation: liquid's cpfskmod (LIQUID_CPFSK_GMSK pulse, modulation index h = 2 * deviation / baud_rate)
//  - demodulation: liquid's freqdem discriminator, then symsync with a gmsk receive filter for
//    matched filtering and timing recovery. liquid's cpfskdem is not used: it assumes perfect
//    timing, and its symbol-spaced phase detector is ambiguous for h >= 1
//  - both chains run at a fixed 8 samples per symbol (cpfskmod needs an even number, and symsync's
//    loop gain depends on it); a resampler bridges from/to the actual sample rate when it differs
//  - settings->bandwidth is the full occupied bandwidth. if not 0, a low-pass filter at half of it
//    is applied to the input before the discriminator
typedef struct gfsk_modem2_t gfsk_modem2;

// sample_rate is the rate the demodulator's DSP chain runs at (i.e. after any decimation the
// caller applied upstream); it may differ from settings->sample_rate, which is always the raw
// input/output rate and is still used as-is for the TX (modulate) chain.
// settings->bt must be in (0, 1]
int gfsk_modem2_create(GfskModemSettings *settings, uint64_t sample_rate, uint32_t max_input_buffer_length, gfsk_modem2 **modem);

void gfsk_modem2_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void gfsk_modem2_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t gfsk_modem2_max_modulation_buffer_length(void *modem);

void gfsk_modem2_destroy(void *modem);

#endif /* DSP_GFSK_MODEM2_H_ */
