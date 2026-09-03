#ifndef DSP_PSK_PM_MODEM_H_
#define DSP_PSK_PM_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct psk_pm_modem_t psk_pm_modem;

typedef struct {
  uint64_t sample_rate;      // sample rate of the input/output I/Q stream, in Hz
  uint32_t baud_rate;        // subcarrier BPSK symbol rate, in Hz. round(sample_rate / baud_rate) must be >= 2.
                              // if sample_rate is not an exact multiple of baud_rate, an arbitrary-rate
                              // resampler is inserted to bridge sample_rate and the nearest working rate
  float rrc_beta;            // root-raised-cosine excess bandwidth (rolloff), 0 < rrc_beta <= 1
  unsigned int rrc_delay;    // root-raised-cosine filter delay, in symbols (m). typically 5-11
  float costas_bandwidth;    // normalized loop bandwidth of the subcarrier costas (carrier recovery) loop, > 0. typically 0.001-0.05
  unsigned int symsync_filter_bank_size; // number of polyphase filters used by the symbol timing recovery loop. typically 16-32

  uint32_t subcarrier_frequency; // frequency of the subcarrier the data is BPSK-modulated onto, in Hz.
                                  // must be well below sample_rate/2 to leave room for the subcarrier's own sidebands
  float modulation_index;        // peak RF carrier phase deviation, in radians, caused by the subcarrier waveform
  float carrier_pll_bandwidth;   // normalized loop bandwidth of the outer RF carrier tracking PLL, > 0. must be much
                                  // smaller than subcarrier_frequency/sample_rate so the loop tracks only slow carrier
                                  // drift and not the subcarrier modulation itself. typically 1e-5 to 1e-3
} psk_pm_modem_settings;

// max_input_buffer_length is the max number of input I/Q samples passed to psk_pm_modem_demodulate() in a
// single call, and (in bytes) the max input passed to psk_pm_modem_modulate() in a single call.
// debug_constellation_file may be NULL, in which case no debug constellation dump is written. otherwise, it
// receives the recovered subcarrier symbols right after symbol timing recovery (demodulate) or the
// subcarrier symbols right before pulse shaping (modulate) -- see bpsk_modem.h.
int psk_pm_modem_create(const psk_pm_modem_settings *settings, uint32_t max_input_buffer_length, const char *debug_constellation_file, psk_pm_modem **modem);

// output is soft-decision bits, one signed byte per bit: sign gives the hard decision (>=0 -> 1,
// < 0 -> 0) and magnitude gives confidence, scaled to the full int8_t range
void psk_pm_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

void psk_pm_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

size_t psk_pm_modem_max_modulation_buffer_length(void *modem);

void psk_pm_modem_destroy(void *modem);

#endif /* DSP_PSK_PM_MODEM_H_ */
