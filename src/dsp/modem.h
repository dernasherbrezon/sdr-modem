#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>
#include "../app_config.h"
#include "freq_offset.h"

typedef struct sdr_modem_t sdr_modem;

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  size_t (*max_modulation_buffer_length)(void *modem);

  void (*destroy)(void *modem);

  // optional. applied to raw I/Q before demodulate and to the modulated output before tx
  freq_offset *freq_offset;
};

// freq_offset_file may be NULL, in which case no frequency correction is applied
int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, sdr_modem **modem);

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t modem_max_modulation_buffer_length(sdr_modem *modem);

void modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
