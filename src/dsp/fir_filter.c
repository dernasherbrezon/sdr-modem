#include "fir_filter.h"
#include <errno.h>
#include <liquid/liquid.h>
#include <string.h>
#include <complex.h>
#include <stdio.h>

int fir_filter_create(uint8_t decimation, float *taps, size_t taps_len,
                      size_t max_input_buffer_length, size_t num_bytes, fir_filter **filter) {
    struct fir_filter_t *result = malloc(sizeof(struct fir_filter_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct fir_filter_t) {0};

    result->decimation = decimation;
    result->num_bytes = num_bytes;
    result->original_taps = taps;
    result->taps_len = taps_len;
    result->history_offset = taps_len - 1;

    if (num_bytes == sizeof(float complex)) {
        result->complex_dotprod = dotprod_crcf_create_rev(taps, (unsigned int) taps_len);
        if (result->complex_dotprod == NULL) {
            fir_filter_destroy(result);
            return -ENOMEM;
        }
    } else if (num_bytes == sizeof(float)) {
        result->real_dotprod = dotprod_rrrf_create_rev(taps, (unsigned int) taps_len);
        if (result->real_dotprod == NULL) {
            fir_filter_destroy(result);
            return -ENOMEM;
        }
    }

    result->max_input_buffer_length = max_input_buffer_length;
    result->working_len_total = max_input_buffer_length + result->history_offset;
    result->working_buffer = malloc(num_bytes * result->working_len_total);
    if (result->working_buffer == NULL) {
        fir_filter_destroy(result);
        return -ENOMEM;
    }
    memset(result->working_buffer, 0, result->working_len_total * num_bytes);

    // +1 for case when round-up needed.
    result->output_len = max_input_buffer_length / decimation + 1;
    result->output = malloc(num_bytes * max_input_buffer_length);
    if (result->output == NULL) {
        fir_filter_destroy(result);
        return -ENOMEM;
    }

    *filter = result;
    return 0;
}

void fir_filter_process_float(const float *input, size_t input_len, float *working_buffer, void **output, size_t *output_len,
                              fir_filter *filter) {
    memcpy(working_buffer + filter->history_offset, input, input_len * filter->num_bytes);
    size_t working_len = filter->history_offset + input_len;
    size_t i = 0;
    size_t produced = 0;
    float *output_pointer = (float *) filter->output;
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; i < max_index; i += filter->decimation, produced++) {
        dotprod_rrrf_execute((dotprod_rrrf) filter->real_dotprod, working_buffer + i, output_pointer);
        output_pointer++;
    }
    filter->history_offset = working_len - i;
    if (i > 0) {
        memmove(working_buffer, working_buffer + i, filter->num_bytes * filter->history_offset);
    }

    *output = filter->output;
    *output_len = produced;
}

float fir_filter_process_float_single(const float *input, fir_filter *filter) {
    float output;
    dotprod_rrrf_execute((dotprod_rrrf) filter->real_dotprod, (float *) input, &output);
    return output;
}

void fir_filter_process_complex(const float complex *input, size_t input_len, float complex *working_buffer, void **output,
                                size_t *output_len, fir_filter *filter) {
    memcpy(working_buffer + filter->history_offset, input, input_len * filter->num_bytes);
    size_t working_len = filter->history_offset + input_len;
    size_t i = 0;
    size_t produced = 0;
    float complex *output_pointer = (float complex *) filter->output;
    size_t max_index = working_len - (filter->taps_len - 1);
    for (; i < max_index; i += filter->decimation, produced++) {
        dotprod_crcf_execute((dotprod_crcf) filter->complex_dotprod, (liquid_float_complex *) (working_buffer + i),
                             (liquid_float_complex *) output_pointer);
        output_pointer++;
    }
    filter->history_offset = working_len - i;
    if (i > 0) {
        memmove(working_buffer, working_buffer + i, filter->num_bytes * filter->history_offset);
    }

    *output = filter->output;
    *output_len = produced;
}

void fir_filter_process(const void *input, size_t input_len, void **output, size_t *output_len, fir_filter *filter) {
    if (input_len > filter->max_input_buffer_length) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, filter->max_input_buffer_length);
        *output = NULL;
        *output_len = 0;
        return;
    }
    if (filter->num_bytes == sizeof(float complex)) {
        fir_filter_process_complex((const float complex *) input, input_len, (float complex *) filter->working_buffer, output,
                                   output_len, filter);
    } else if (filter->num_bytes == sizeof(float)) {
        fir_filter_process_float((const float *) input, input_len, (float *) filter->working_buffer, output, output_len, filter);
    }
}

void fir_filter_destroy(fir_filter *filter) {
    if (filter == NULL) {
        return;
    }
    if (filter->real_dotprod != NULL) {
        dotprod_rrrf_destroy((dotprod_rrrf) filter->real_dotprod);
    }
    if (filter->complex_dotprod != NULL) {
        dotprod_crcf_destroy((dotprod_crcf) filter->complex_dotprod);
    }
    if (filter->original_taps != NULL) {
        free(filter->original_taps);
    }
    if (filter->output != NULL) {
        free(filter->output);
    }
    if (filter->working_buffer != NULL) {
        free(filter->working_buffer);
    }
    free(filter);
}
