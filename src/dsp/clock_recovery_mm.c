#include "clock_recovery_mm.h"
#include "mmse_fir_interpolator.h"
#include <errno.h>
#include <string.h>
#include <math.h>
#include <volk/volk.h>

struct clock_mm_t {
    mmse_fir_interpolator *interp;
    float omega;
    float omega_mid;
    float omega_lim;
    float gain_omega;
    float mu;
    float gain_mu;
    float omega_relative_limit;
    float last_sample;

    float *working_buffer;
    size_t history_offset;
    size_t working_len_total;

    float *output;
    size_t output_len;
};

int
clock_mm_create(float omega, float gain_omega, float mu, float gain_mu, float omega_relative_limit, size_t output_len,
                clock_mm **clock) {
    struct clock_mm_t *result = malloc(sizeof(struct clock_mm_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct clock_mm_t) {0};
    result->mu = mu;
    result->omega = omega;
    result->gain_omega = gain_omega;
    result->gain_mu = gain_mu;
    result->omega_relative_limit = omega_relative_limit;
    result->omega_mid = omega;
    result->omega_lim = result->omega_mid * result->omega_relative_limit;

    result->last_sample = 0.0F;
    int code = mmse_fir_interpolator_create(&result->interp);
    if (code != 0) {
        clock_mm_destroy(result);
        return code;
    }
    result->output_len = output_len;
    result->output = malloc(sizeof(float) * output_len);
    if (result->output == NULL) {
        clock_mm_destroy(result);
        return -ENOMEM;
    }
    result->history_offset = 0;
    result->working_len_total = output_len + mmse_fir_interpolator_taps(result->interp);
    result->working_buffer = volk_malloc(sizeof(float) * result->working_len_total, volk_get_alignment());
    if (result->working_buffer == NULL) {
        clock_mm_destroy(result);
        return -ENOMEM;
    }

    *clock = result;
    return 0;
}

float slice(float x) {
    return x < 0 ? -1.0F : 1.0F;
}

static inline float branchless_clip(float x, float clip) {
    return 0.5F * (fabsf(x + clip) - fabsf(x - clip));
}

void clock_mm_process(const float *input, size_t input_len, float **output, size_t *output_len, clock_mm *clock) {
    // prepend history to the input
    memcpy(clock->working_buffer + clock->history_offset, input, input_len * sizeof(float));
    int taps_len = mmse_fir_interpolator_taps(clock->interp);
    int ii = 0;                                  // input index
    int oo = 0;                                  // output index
    int previous = 0;
    size_t working_len = clock->history_offset + input_len;
    size_t max_index = working_len - (taps_len - 1);
    float mm_val;

    while (ii < max_index) {
        // produce output sample
        clock->output[oo] = mmse_fir_interpolator_process(clock->working_buffer + ii, taps_len, clock->mu,
                                                          clock->interp);
        mm_val = slice(clock->last_sample) * clock->output[oo] - slice(clock->output[oo]) * clock->last_sample;
        clock->last_sample = clock->output[oo];
        previous = ii;

        clock->omega = clock->omega + clock->gain_omega * mm_val;
        clock->omega = clock->omega_mid + branchless_clip(clock->omega - clock->omega_mid, clock->omega_lim);
        clock->mu = clock->mu + clock->omega + clock->gain_mu * mm_val;
        ii += (int) floorf(clock->mu);
        clock->mu = clock->mu - floorf(clock->mu);
        oo++;
    }

    size_t last_index;
    if (ii > working_len) {
        last_index = previous;
    } else {
        last_index = ii;
    }
    clock->history_offset = working_len - last_index;

    memmove(clock->working_buffer, clock->working_buffer + last_index, sizeof(float) * clock->history_offset);

    *output = clock->output;
    *output_len = oo;
}

void clock_mm_destroy(clock_mm *clock) {
    if (clock == NULL) {
        return;
    }
    if (clock->interp != NULL) {
        mmse_fir_interpolator_destroy(clock->interp);
    }
    if (clock->output != NULL) {
        free(clock->output);
    }
    if (clock->working_buffer != NULL) {
        volk_free(clock->working_buffer);
    }
    free(clock);
}
