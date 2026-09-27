#ifndef DSP_GFSK_MODEM_H_
#define DSP_GFSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>
#include "../api.pb-c.h"

// GFSK modem built on liquid-dsp's continuous-phase FSK (cpfskmod) with a gaussian frequency pulse.
// Soft-symbol output convention: int8, positive = bit 1.
//
//  - modulation: liquid's cpfskmod (LIQUID_CPFSK_GMSK pulse, modulation index h = 2 * deviation / baud_rate)
//  - with a sync word demodulation is data-aided and meant for bursts: liquid's freqdem discriminator
//    and a gmsk receive filter, then a correlator looks for the sync word (either polarity). its peak
//    gives the symbol timing, the frequency offset (dc) and the amplitude of the burst, which are then
//    held for the whole burst: there is no timing loop and no dc blocker, so settings->use_dc_block is
//    ignored. soft symbols are normalized by the amplitude of the sync word, so +-127 does not depend
//    on the deviation. the output is continuous: between bursts the last estimates are kept. the sync
//    word itself is included in the output. output lags the input by ~41 symbols (sync word + peak search)
//  - without a sync word (syncword_bits = 0) demodulation is continuous instead: the discriminator
//    output goes through an optional dc blocker (settings->use_dc_block) and liquid's symsync with a
//    gmsk receive filter bank. its timing error detector assumes h = 0.5 (gmsk). soft symbols are
//    scaled by the configured deviation (+-127 = +-deviation)
//  - both chains run at a fixed 2 samples per symbol (cpfskmod needs an even number, and the
//    correlator template is built at it); a resampler bridges from/to the actual sample rate
//  - settings->bandwidth is the full occupied bandwidth. if not 0, a low-pass filter at half of it
//    is applied to the input before the discriminator. it is skipped when the bandwidth is at or above
//    the internal sample rate (h >= 1): the input is already band-limited to it
typedef struct gfsk_modem_t gfsk_modem;

// sample_rate is the rate the demodulator's DSP chain runs at (i.e. after any decimation the
// caller applied upstream); it may differ from settings->sample_rate, which is always the raw
// input/output rate and is still used as-is for the TX (modulate) chain.
// settings->bt must be in (0, 1]
// syncword - on-air symbols, msb first (see ModemRequest.syncword); syncword_bits must be in [0, 64].
//            0 means no sync word: symbol timing is recovered by symsync
int gfsk_modem_create(GfskModemSettings *settings, uint64_t syncword, uint32_t syncword_bits, uint64_t sample_rate, uint32_t max_input_buffer_length, gfsk_modem **modem);

void gfsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void gfsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t gfsk_modem_max_modulation_buffer_length(void *modem);

void gfsk_modem_destroy(void *modem);

#endif /* DSP_GFSK_MODEM_H_ */
