#ifndef SDR_MODEM_SIG_SOURCE_H
#define SDR_MODEM_SIG_SOURCE_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct sig_source_t sig_source;

int sig_source_create(float amplitude, uint32_t rx_sampling_freq, uint32_t max_output_buffer_length, sig_source **source);

void sig_source_process(int32_t freq, size_t expected_output_len, float complex **output, size_t *output_len, sig_source *source);

void sig_source_multiply(int32_t freq, float complex *input, size_t input_len, float complex **output, size_t *output_len, sig_source *source);

void sig_source_destroy(sig_source *source);

#endif //SDR_MODEM_SIG_SOURCE_H
