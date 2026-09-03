#ifndef DSP_OQPSK_MODEM_H_
#define DSP_OQPSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct oqpsk_modem_t oqpsk_modem;

typedef struct {
  uint64_t sample_rate;      // sample rate of the input/output I/Q stream, in Hz
  uint32_t baud_rate;        // symbol rate, in Hz. round(sample_rate / baud_rate) must be >= 2.
                              // if sample_rate is not an exact multiple of baud_rate, an arbitrary-rate
                              // resampler is inserted to bridge sample_rate and the nearest working rate
  float rrc_beta;            // root-raised-cosine excess bandwidth (rolloff), 0 < rrc_beta <= 1
  unsigned int rrc_delay;    // root-raised-cosine filter delay, in symbols (m). typically 5-11
  float costas_bandwidth;    // normalized loop bandwidth of the costas (carrier recovery) loop, > 0. typically 0.001-0.05
} oqpsk_modem_settings;

// max_input_buffer_length is the max number of input I/Q samples passed to oqpsk_modem_demodulate() in a
// single call, and (in bytes) the max input passed to oqpsk_modem_modulate() in a single call.
// debug_constellation_file may be NULL, in which case no debug constellation dump is written. otherwise, it
// receives the de-rotated I/Q samples right after carrier recovery (demodulate) or the I/Q symbols right
// before pulse shaping (modulate).
int oqpsk_modem_create(const oqpsk_modem_settings *settings, uint32_t max_input_buffer_length, const char *debug_constellation_file, oqpsk_modem **modem);

// output is soft-decision bits, one signed byte per bit, MSB-first pairs of (I, Q) per symbol, matching
// the bit order used by oqpsk_modem_modulate(): sign gives the hard decision (>=0 -> 1, < 0 -> 0) and
// magnitude gives confidence, scaled to the full int8_t range
void oqpsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void oqpsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t oqpsk_modem_max_modulation_buffer_length(void *modem);

void oqpsk_modem_destroy(void *modem);

#endif /* DSP_OQPSK_MODEM_H_ */
