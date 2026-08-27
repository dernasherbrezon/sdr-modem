#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../app_config.h"
#include "freq_offset.h"
#include "halfband_decim.h"

typedef struct sdr_modem_t sdr_modem;

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  size_t (*max_modulation_buffer_length)(void *modem);

  void (*destroy)(void *modem);

  // optional. decimates raw I/Q down to just above the signal bandwidth before demodulate, so the
  // wrapped modem's own DSP chain runs at a lower, cheaper sample rate
  halfband_decim *halfband;

  // optional. applied to raw I/Q before demodulate and to the modulated output before tx
  freq_offset *freq_offset;

  // optional. dumps raw I/Q samples for debugging: on rx, the (possibly freq_offset-corrected)
  // samples right before demodulate; on tx, the modulated samples right after modulate, before
  // freq_offset correction is applied
  FILE *debug_freq_offset_file;

  // optional. rx only: dumps the baseband I/Q samples right after halfband decimation
  // (i.e. right before the wrapped demodulator runs), or the freq_offset-corrected raw
  // input when no halfband decimation is configured
  FILE *debug_baseband_file;
};

// freq_offset_file may be NULL, in which case no frequency correction is applied
// debug_freq_offset_file may be NULL, in which case no debug I/Q dump is written
// debug_constellation_file may be NULL, in which case no debug constellation dump is written.
// only honored by bpsk/dpsk/sdpsk modems.
// debug_baseband_file may be NULL, in which case no debug baseband dump is written. rx only.
int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, const char *debug_freq_offset_file, const char *debug_constellation_file, const char *debug_baseband_file, sdr_modem **modem);

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t modem_max_modulation_buffer_length(sdr_modem *modem);

void modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
