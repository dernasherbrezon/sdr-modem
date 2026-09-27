#include "halfband_interp.h"
#include <errno.h>
#include <stdio.h>
#include <liquid/liquid.h>

struct halfband_interp_t {
    msresamp2_crcf resampler;
    unsigned int interpolation;
    size_t max_input_buffer_length;

    float complex *output;
    size_t output_len;
};

int halfband_interp_create(unsigned int num_stages, float fc, float stopband_attenuation_db,
                           uint32_t max_input_buffer_length, halfband_interp **filter) {
    struct halfband_interp_t *result = malloc(sizeof(struct halfband_interp_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct halfband_interp_t) {0};
    result->interpolation = 1u << num_stages;

    result->resampler = msresamp2_crcf_create(LIQUID_RESAMP_INTERP, num_stages, fc, 0.0f, stopband_attenuation_db);
    if (result->resampler == NULL) {
        halfband_interp_destroy(result);
        return -EINVAL;
    }

    result->max_input_buffer_length = max_input_buffer_length;
    result->output_len = (size_t) max_input_buffer_length * result->interpolation;
    result->output = malloc(sizeof(float complex) * result->output_len);
    if (result->output == NULL) {
        halfband_interp_destroy(result);
        return -ENOMEM;
    }

    *filter = result;
    return 0;
}

void halfband_interp_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len,
                             halfband_interp *filter) {
    if (input_len > filter->max_input_buffer_length) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, filter->max_input_buffer_length);
        *output = NULL;
        *output_len = 0;
        return;
    }
    // every input sample produces exactly "interpolation" output samples, so no history is needed
    for (size_t i = 0; i < input_len; i++) {
        msresamp2_crcf_execute(filter->resampler,
                               (liquid_float_complex *) (input + i),
                               (liquid_float_complex *) (filter->output + i * filter->interpolation));
    }

    *output = filter->output;
    *output_len = input_len * filter->interpolation;
}

size_t halfband_interp_max_output_buffer_length(halfband_interp *filter) {
    return filter->output_len;
}

void halfband_interp_destroy(halfband_interp *filter) {
    if (filter == NULL) {
        return;
    }
    if (filter->resampler != NULL) {
        msresamp2_crcf_destroy(filter->resampler);
    }
    if (filter->output != NULL) {
        free(filter->output);
    }
    free(filter);
}
