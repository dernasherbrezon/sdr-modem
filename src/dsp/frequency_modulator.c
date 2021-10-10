#include "frequency_modulator.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <volk/volk.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct frequency_modulator_t {
    float phase;
    float sensitivity;

    float *temp;
    size_t temp_len;

    float *sin;
    size_t sin_len;

    float *cos;
    size_t cos_len;

    float complex *output;
    size_t output_len;
};

int frequency_modulator_create(float sensitivity, uint32_t max_input_buffer_length, frequency_modulator **mod) {
    struct frequency_modulator_t *result = malloc(sizeof(struct frequency_modulator_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct frequency_modulator_t) {0};
    result->phase = 0;
    result->sensitivity = sensitivity;

    result->output_len = max_input_buffer_length;
    result->output = malloc(sizeof(float complex) * result->output_len);
    if (result->output == NULL) {
        frequency_modulator_destroy(result);
        return -ENOMEM;
    }
    result->sin_len = max_input_buffer_length;
    result->sin = malloc(sizeof(float) * result->sin_len);
    if (result->sin == NULL) {
        frequency_modulator_destroy(result);
        return -ENOMEM;
    }
    result->cos_len = max_input_buffer_length;
    result->cos = malloc(sizeof(float) * result->cos_len);
    if (result->cos == NULL) {
        frequency_modulator_destroy(result);
        return -ENOMEM;
    }
    result->temp_len = max_input_buffer_length;
    result->temp = malloc(sizeof(float) * result->temp_len);
    if (result->temp == NULL) {
        frequency_modulator_destroy(result);
        return -ENOMEM;
    }


    *mod = result;
    return 0;
}

void frequency_modulator_process(float *input, size_t input_len, float complex **output, size_t *output_len, frequency_modulator *mod) {
    if (input_len > mod->output_len) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, mod->output_len);
        *output = NULL;
        *output_len = 0;
        return;
    }
    for (size_t i = 0; i < input_len; i++) {
        mod->phase = mod->phase + mod->sensitivity * input[i];
        mod->temp[i] = mod->phase;
    }

    volk_32f_sin_32f(mod->sin, mod->temp, (unsigned int) input_len);
    volk_32f_cos_32f(mod->cos, mod->temp, (unsigned int) input_len);

    volk_32f_x2_interleave_32fc(mod->output, mod->cos, mod->sin, (unsigned int) input_len);

    *output = mod->output;
    *output_len = input_len;
}

void frequency_modulator_destroy(frequency_modulator *mod) {
    if (mod == NULL) {
        return;
    }
    if (mod->output != NULL) {
        free(mod->output);
    }
    if (mod->temp != NULL) {
        free(mod->temp);
    }
    if (mod->sin != NULL) {
        free(mod->sin);
    }
    if (mod->cos != NULL) {
        free(mod->cos);
    }
    free(mod);
}