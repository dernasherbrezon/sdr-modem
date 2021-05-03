#ifndef DSP_QUADRATURE_DEMOD_H_
#define DSP_QUADRATURE_DEMOD_H_

#include <complex.h>
#include <stdint.h>

typedef struct quadrature_demod_t quadrature_demod;

int quadrature_demod_create(float gain, uint32_t max_input_buffer_length, quadrature_demod **demod);

void quadrature_demod_process(float complex *input, size_t input_len, float **output, size_t *output_len, quadrature_demod *demod);

void quadrature_demod_destroy(quadrature_demod *demod);

#endif /* DSP_QUADRATURE_DEMOD_H_ */
