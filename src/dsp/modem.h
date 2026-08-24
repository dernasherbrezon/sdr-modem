#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../app_config.h"

typedef struct sdr_modem_t sdr_modem;

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);
  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);
  size_t (*max_modulation_buffer_length)(void *modem);
  void (*destroy)(void *modem);
};

int modem_create(app_config *config, bool is_tx, sdr_modem **modem);

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t modem_max_modulation_buffer_length(sdr_modem *modem);

void modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
