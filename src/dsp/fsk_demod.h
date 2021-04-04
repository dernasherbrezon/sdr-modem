#ifndef DSP_FSK_DEMOD_H_
#define DSP_FSK_DEMOD_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct fsk_demod_t fsk_demod;

int create_fsk_demod(uint32_t sampling_freq, int baud_rate, float deviation, int decimation, uint32_t transition_width, bool use_dc_block, fsk_demod **demod);

void fsk_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod);

int destroy_fsk_demod(fsk_demod *demod);

#endif /* DSP_FSK_DEMOD_H_ */
