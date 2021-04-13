#include "utils.h"
#include <check.h>
#include <volk/volk.h>
#include <stdio.h>

void setup_input_data(float **input, size_t input_offset, size_t len) {
    float *result = malloc(sizeof(float) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (input_offset + i);
    }
    *input = result;
}

void setup_volk_input_data(float **input, size_t input_offset, size_t len) {
    float *result = volk_malloc(sizeof(float) * len, volk_get_alignment());
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (input_offset + i);
    }
    *input = result;
}

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len) {
    float complex *result = malloc(sizeof(float complex) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (2 * input_offset + 2 * i) + (float) (2 * input_offset + 2 * i + 1) * I;
    }
    *input = result;
}

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
        ck_assert(fabsl(expected[i] - crealf(actual[j])) < 0.001);
        ck_assert(fabsl(expected[i + 1] - cimagf(actual[j])) < 0.001);
    }
}

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; i++) {
        ck_assert(fabsl(expected[i] - actual[i]) < 0.001);
    }
}