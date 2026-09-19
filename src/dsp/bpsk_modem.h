#ifndef DSP_BPSK_MODEM_H_
#define DSP_BPSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct bpsk_modem_t bpsk_modem;

typedef enum {
  BPSK,
  SDPSK,
  DPSK
} psk_modem_type;

typedef struct {
  uint64_t sample_rate;
  uint32_t baud_rate;        // round(sample_rate / baud_rate) must be >= 2.
  uint32_t bandwidth;
  float rrc_beta;            // root-raised-cosine excess bandwidth (rolloff), 0 < rrc_beta <= 1
  unsigned int rrc_delay;    // root-raised-cosine filter delay, in symbols (m). typically 5-11
  float costas_bandwidth;    // normalized loop bandwidth of the costas (carrier recovery) loop, > 0. typically 0.001-0.05
  unsigned int symsync_filter_bank_size; // number of polyphase filters used by the symbol timing recovery loop. typically 16-32
  psk_modem_type type;
} bpsk_modem_settings;

int bpsk_modem_create(const bpsk_modem_settings *settings, uint32_t max_input_buffer_length, bpsk_modem **modem);

int bpsk_modem_set_debug_constellation_file(bpsk_modem *modem, const char *debug_constellation_file);

void bpsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void bpsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t bpsk_modem_max_modulation_buffer_length(void *modem);

void bpsk_modem_destroy(void *modem);

#endif /* DSP_BPSK_MODEM_H_ */
