#ifndef DSP_PSK_PM_MODEM_H_
#define DSP_PSK_PM_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct psk_pm_modem_t psk_pm_modem;

typedef struct {
  uint64_t sample_rate; // sample rate of the input/output I/Q stream, in Hz
  uint32_t subcarrier_frequency; // frequency of the subcarrier the data is BPSK-modulated onto, in Hz.
  float carrier_pll_bandwidth; // normalized loop bandwidth of the outer RF carrier tracking PLL, > 0

  uint32_t baud_rate;
  // cutoff frequency, in Hz, of an optional low-pass filter placed ahead of the internal
  // subcarrier BPSK demodulator's AGC/symbol synchronizer (see bpsk_modem_settings.subcarrier_bandwidth).
  // 0 disables the filter.
  uint32_t subcarrier_bandwidth;
  float rrc_beta; // root-raised-cosine excess bandwidth (rolloff), 0 < rrc_beta <= 1
  unsigned int rrc_delay; // root-raised-cosine filter delay, in symbols (m). typically 5-11
  float costas_bandwidth; // normalized loop bandwidth of the subcarrier costas (carrier recovery) loop, > 0. typically 0.001-0.05
  unsigned int symsync_filter_bank_size; // number of polyphase filters used by the symbol timing recovery loop. typically 16-32

  float modulation_index; // peak RF carrier phase deviation, in radians, caused by the subcarrier waveform
} psk_pm_modem_settings;

int psk_pm_modem_create(const psk_pm_modem_settings *settings, uint32_t max_input_buffer_length, const char *debug_subcarrier_file, psk_pm_modem **modem);

int psk_pm_modem_set_debug_constellation_file(const char *debug_constellation_file, psk_pm_modem *modem);

void psk_pm_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void psk_pm_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t psk_pm_modem_max_modulation_buffer_length(void *modem);

void psk_pm_modem_destroy(void *modem);

#endif /* DSP_PSK_PM_MODEM_H_ */
