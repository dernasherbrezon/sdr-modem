#ifndef SDR_MODEM_INTERP_FIR_FILTER_H
#define SDR_MODEM_INTERP_FIR_FILTER_H

#include <stdlib.h>
#include <stdint.h>

typedef struct interp_fir_filter_t interp_fir_filter;

int interp_fir_filter_create(float *taps, size_t taps_len, uint8_t interpolation, uint32_t max_input_buffer_length, interp_fir_filter **filter);

void interp_fir_filter_process(float *input, size_t input_len, float **output, size_t *output_len, interp_fir_filter *filter);

void interp_fir_filter_destroy(interp_fir_filter *filter);

#endif //SDR_MODEM_INTERP_FIR_FILTER_H
