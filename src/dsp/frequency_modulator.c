#include "frequency_modulator.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct frequency_modulator_t {
    float phase;
    float sensitivity;

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
        mod->phase = fmodf(mod->phase + M_PI, 2.0f * M_PI) - M_PI;

        //FIXME find fastest sincos
//        int32_t angle = gr::fxpt::float_to_fixed(d_phase);
//        gr::fxpt::sincos(angle, &oq, &oi);
//        out[i] = gr_complex(oi, oq);
    }
}

void frequency_modulator_destroy(frequency_modulator *mod) {
    if (mod == NULL) {
        return;
    }
    if (mod->output != NULL) {
        free(mod->output);
    }
    free(mod);
}