#ifndef DSP_GFSK_MODEM_H_
#define DSP_GFSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct gfsk_modem_t gfsk_modem;

typedef struct {
  uint64_t sample_rate;      // sample rate of the input/output I/Q stream, in Hz. sample_rate / baud_rate must be >= 2
  uint32_t baud_rate;
  int64_t deviation;         // frequency deviation, in Hz. must not be 0
  uint32_t bandwidth;        // full occupied bandwidth, in Hz. 0 disables the input low-pass filter
  float bt;                  // gaussian filter bandwidth-time product, in (0, 1]
  uint8_t use_dc_block;         // remove the frequency offset. used only without a sync word
  uint64_t syncword;         // use syncowrd for data-aided demodulation and short bursts
  uint32_t syncword_bits;    // number of sync word bits, in [0, 64]. 0 means no sync word: symbol timing is recovered by symsync
} gfsk_modem_settings;

int gfsk_modem_create(const gfsk_modem_settings *settings, uint32_t max_input_buffer_length, gfsk_modem **modem);

void gfsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void gfsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

// pushes the remaining tail of the modulated signal out and resets the tx state, so that the next
// modulate starts a new independent transmission. output_len is 0 if nothing was modulated since the last flush.
// the output buffer is the same as of modulate: consume the modulate output first
void gfsk_modem_modulate_flush(float complex **output, size_t *output_len, void *modem);

// carrier frequency offset of the received signal, in Hz (positive: the signal is above the center).
// with a sync word it is the offset estimated from the latest burst (0 until the first one).
// without a sync word it is the dc blocker estimate, available only when use_dc_block is set.
// otherwise the offset is not estimated and the result is always 0
float gfsk_modem_get_frequency_offset(void *modem);

size_t gfsk_modem_max_modulation_buffer_length(void *modem);

void gfsk_modem_destroy(void *modem);

#endif /* DSP_GFSK_MODEM_H_ */
