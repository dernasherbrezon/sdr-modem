#include <stdlib.h>
#include <volk/volk.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "quadrature_demod.h"
#include "../math/fast_atan2f.h"

struct quadrature_demod_t {
    float gain;

    float complex *temp_buffer;
    size_t temp_buffer_len;

    float complex *working_buffer;
    size_t history_offset;
    size_t working_buffer_len;

    float *output;
    size_t output_len;
};

int quadrature_demod_create(float gain, uint32_t max_input_buffer_length, quadrature_demod **demod) {
    struct quadrature_demod_t *result = malloc(sizeof(struct quadrature_demod_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct quadrature_demod_t) {0};
    result->gain = gain;
    result->output_len = max_input_buffer_length;
    result->output = malloc(sizeof(float) * result->output_len);
    if (result->output == NULL) {
        quadrature_demod_destroy(result);
        return -ENOMEM;
    }
    result->history_offset = 1;
    result->working_buffer_len = result->output_len + result->history_offset;
    result->working_buffer = malloc(sizeof(float complex) * result->working_buffer_len);
    if (result->working_buffer == NULL) {
        quadrature_demod_destroy(result);
        return -ENOMEM;
    }
    memset(result->working_buffer, 0, result->working_buffer_len * sizeof(float complex));

    result->temp_buffer_len = max_input_buffer_length;
    result->temp_buffer = malloc(sizeof(float complex) * result->temp_buffer_len);
    if (result->temp_buffer == NULL) {
        quadrature_demod_destroy(result);
        return -ENOMEM;
    }

    *demod = result;
    return 0;
}

void quadrature_demod_process(float complex *input, size_t input_len, float **output, size_t *output_len, quadrature_demod *demod) {
    if (input_len > demod->output_len) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, demod->output_len);
        *output = NULL;
        *output_len = 0;
        return;
    }
    memcpy(demod->working_buffer + demod->history_offset, input, sizeof(float complex) * input_len);
    volk_32fc_x2_multiply_conjugate_32fc(demod->temp_buffer, &demod->working_buffer[1], &demod->working_buffer[0], input_len);
    for (int i = 0; i < input_len; i++) {
        demod->output[i] = demod->gain * fast_atan2f(cimagf(demod->temp_buffer[i]), crealf(demod->temp_buffer[i]));
    }
    memmove(demod->working_buffer, demod->working_buffer + input_len, sizeof(float complex) * demod->history_offset);

    *output = demod->output;
    *output_len = input_len;
}

void quadrature_demod_destroy(quadrature_demod *demod) {
    if (demod == NULL) {
        return;
    }
    if (demod->output != NULL) {
        free(demod->output);
    }
    if (demod->working_buffer != NULL) {
        free(demod->working_buffer);
    }
    if (demod->temp_buffer != NULL) {
        free(demod->temp_buffer);
    }
    free(demod);
}
