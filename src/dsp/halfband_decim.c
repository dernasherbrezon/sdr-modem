#include "halfband_decim.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <liquid/liquid.h>

struct halfband_decim_t {
    msresamp2_crcf resampler;
    unsigned int decimation;

    float complex *working_buffer;
    size_t history_offset;
    size_t max_input_buffer_length;

    float complex *output;
    size_t output_len;
};

int halfband_decim_create(unsigned int num_stages, float fc, float stopband_attenuation_db,
                          uint32_t max_input_buffer_length, halfband_decim **filter) {
    struct halfband_decim_t *result = malloc(sizeof(struct halfband_decim_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct halfband_decim_t) {0};
    result->decimation = 1u << num_stages;

    result->resampler = msresamp2_crcf_create(LIQUID_RESAMP_DECIM, num_stages, fc, 0.0f, stopband_attenuation_db);
    if (result->resampler == NULL) {
        halfband_decim_destroy(result);
        return -EINVAL;
    }

    result->max_input_buffer_length = max_input_buffer_length;
    result->working_buffer = malloc(sizeof(float complex) * (max_input_buffer_length + result->decimation));
    if (result->working_buffer == NULL) {
        halfband_decim_destroy(result);
        return -ENOMEM;
    }
    memset(result->working_buffer, 0, sizeof(float complex) * (max_input_buffer_length + result->decimation));

    result->output_len = max_input_buffer_length / result->decimation + 1;
    result->output = malloc(sizeof(float complex) * result->output_len);
    if (result->output == NULL) {
        halfband_decim_destroy(result);
        return -ENOMEM;
    }

    *filter = result;
    return 0;
}

void halfband_decim_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len,
                            halfband_decim *filter) {
    if (input_len > filter->max_input_buffer_length) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, filter->max_input_buffer_length);
        *output = NULL;
        *output_len = 0;
        return;
    }
    memcpy(filter->working_buffer + filter->history_offset, input, sizeof(float complex) * input_len);
    size_t working_len = filter->history_offset + input_len;
    size_t num_blocks = working_len / filter->decimation;
    if (num_blocks > filter->output_len) {
        num_blocks = filter->output_len;
    }
    for (size_t i = 0; i < num_blocks; i++) {
        msresamp2_crcf_execute(filter->resampler,
                               (liquid_float_complex *) (filter->working_buffer + i * filter->decimation),
                               (liquid_float_complex *) (filter->output + i));
    }
    size_t consumed = num_blocks * filter->decimation;
    filter->history_offset = working_len - consumed;
    if (consumed > 0) {
        memmove(filter->working_buffer, filter->working_buffer + consumed, sizeof(float complex) * filter->history_offset);
    }

    *output = filter->output;
    *output_len = num_blocks;
}

void halfband_decim_destroy(halfband_decim *filter) {
    if (filter == NULL) {
        return;
    }
    if (filter->resampler != NULL) {
        msresamp2_crcf_destroy(filter->resampler);
    }
    if (filter->output != NULL) {
        free(filter->output);
    }
    if (filter->working_buffer != NULL) {
        free(filter->working_buffer);
    }
    free(filter);
}
