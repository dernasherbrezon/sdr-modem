#ifndef DSP_GMSK_MODEM_H_
#define DSP_GMSK_MODEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>
#include "../api.pb-c.h"

typedef struct gmsk_modem_t gmsk_modem;

int gmsk_modem_create(GmskModemSettings *settings, uint32_t max_input_buffer_length, gmsk_modem **demod);

void gmsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *demod);

void gmsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *mod);

//used in tests
int gmsk_modem_convolve(float *x, size_t x_len, float *y, size_t y_len, float **out, size_t *out_len);

void gmsk_modem_destroy(void *demod);

#endif /* DSP_GMSK_MODEM_H_ */
