#ifndef SDR_MODEM_FIR_FILTER_H
#define SDR_MODEM_FIR_FILTER_H

#include <stdlib.h>

typedef struct fir_filter_t fir_filter;

struct fir_filter_t {
    int decimation;

    float **taps;
    size_t aligned_taps_len;
    size_t alignment;
    size_t taps_len;
    float *original_taps;

    void *working_buffer;
    size_t history_offset;
    size_t working_len_total;
    void *volk_output;

    void *output;
    size_t output_len;
    size_t num_bytes;
};

int fir_filter_create(int decimation, float *taps, size_t taps_len, size_t output_len, size_t num_bytes, fir_filter **filter);

void fir_filter_process(const void *input, size_t input_len, void **output, size_t *output_len, fir_filter *filter);

float fir_filter_process_float_single(const float *input, size_t input_len, fir_filter *filter);

void fir_filter_destroy(fir_filter *filter);


#endif //SDR_MODEM_FIR_FILTER_H
