#ifndef DSP_FSK_DEMOD_H_
#define DSP_FSK_DEMOD_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct fsk_demod_t fsk_demod;

int fsk_demod_create(uint32_t sampling_freq, int baud_rate, float deviation, uint8_t decimation, uint32_t transition_width, bool use_dc_block, uint32_t max_input_buffer_length, fsk_demod **demod);

void fsk_demod_process(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod);

void fsk_demod_destroy(fsk_demod *demod);

#endif /* DSP_FSK_DEMOD_H_ */
