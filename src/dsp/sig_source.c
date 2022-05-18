#include "sig_source.h"
#include <math.h>
#include <errno.h>
#include <volk/volk.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_2PI ((float) (2 * M_PI))

struct sig_source_t {
    float phase;
    float amplitude;

    float complex *output;
    uint32_t output_len;
    uint64_t rx_sampling_freq;
};

int sig_source_create(float amplitude, uint64_t rx_sampling_freq, uint32_t max_output_buffer_length, sig_source **source) {
    struct sig_source_t *result = malloc(sizeof(struct sig_source_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct sig_source_t) {0};
    result->phase = 0.0F;
    result->amplitude = amplitude;
    result->rx_sampling_freq = rx_sampling_freq;
    result->output_len = max_output_buffer_length;
    result->output = malloc(sizeof(float complex) * result->output_len);
    if (result->output == NULL) {
        sig_source_destroy(result);
        return -ENOMEM;
    }

    *source = result;
    return 0;
}

void sig_source_process(int64_t freq, size_t expected_output_len, float complex **output, size_t *output_len, sig_source *source) {
    float adjusted_freq = M_2PI * (float) freq / source->rx_sampling_freq;
    for (size_t i = 0; i < expected_output_len; i++) {
        source->output[i] = cos(source->phase) * source->amplitude + sin(source->phase) * source->amplitude * I;
        source->phase += adjusted_freq;
        if (source->phase < -M_2PI) {
            source->phase += M_2PI;
        }
        if (source->phase > M_2PI) {
            source->phase -= M_2PI;
        }
    }

    *output = source->output;
    *output_len = expected_output_len;
}

void sig_source_multiply(int64_t freq, const float complex *input, size_t input_len, float complex **output, size_t *output_len, sig_source *source) {
    if (input_len > source->output_len) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %u\n", input_len, source->output_len);
        *output = NULL;
        *output_len = 0;
        return;
    }
    float complex *sig_output = NULL;
    size_t sig_output_len = 0;
    sig_source_process(freq, input_len, &sig_output, &sig_output_len, source);

    volk_32fc_x2_multiply_32fc(source->output, (const lv_32fc_t *) input, (const lv_32fc_t *) sig_output, (unsigned int) input_len);

    *output = source->output;
    *output_len = input_len;
}

void sig_source_destroy(sig_source *source) {
    if (source == NULL) {
        return;
    }
    if (source->output != NULL) {
        free(source->output);
    }
    free(source);
}