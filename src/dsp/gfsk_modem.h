#ifndef DSP_GFSK_MODEM_H_
#define DSP_GFSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>
#include "../api.pb-c.h"

typedef struct gfsk_modem_t gfsk_modem;

int gfsk_modem_create(GfskModemSettings *settings, uint32_t max_input_buffer_length, gfsk_modem **demod);

void gfsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *demod);

void gfsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *mod);

size_t gfsk_modem_max_modulation_buffer_length(void *mod);

//used in tests
int gfsk_modem_convolve(float *x, size_t x_len, float *y, size_t y_len, float **out, size_t *out_len);

void gfsk_modem_destroy(void *demod);

#endif /* DSP_GFSK_MODEM_H_ */
