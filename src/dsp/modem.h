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
};

// number of half-band decimation stages needed so the decimated sample rate stays at least
// MODEM_HALFBAND_MIN_OVERSAMPLE times the signal bandwidth. returns 0 if no decimation is needed.
unsigned int modem_estimate_halfband_stages(uint64_t sample_rate, uint32_t bandwidth);

// creates a half-band decimator sized for sample_rate/bandwidth, if decimation is required.
// on return, *halfband is NULL and *decimated_sample_rate/*decimated_max_input_buffer_length are
// left equal to sample_rate/max_input_buffer_length when no decimation is needed.
int modem_halfband_decim_create(uint64_t sample_rate, uint32_t bandwidth, uint32_t max_input_buffer_length,
                                halfband_decim **halfband, uint64_t *decimated_sample_rate,
                                uint32_t *decimated_max_input_buffer_length);

// freq_offset_file may be NULL, in which case no frequency correction is applied
// debug_freq_offset_file may be NULL, in which case no debug I/Q dump is written
int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, const char *debug_freq_offset_file, sdr_modem **modem);

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t modem_max_modulation_buffer_length(sdr_modem *modem);

void modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
