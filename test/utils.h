#ifndef SDR_MODEM_UTILS_H
#define SDR_MODEM_UTILS_H

#include <complex.h>
#include <stdlib.h>
#include "../src/api.h"
#include <stdio.h>

struct request *create_request();

void setup_input_data(float **input, size_t input_offset, size_t len);

void setup_volk_input_data(float **input, size_t input_offset, size_t len);

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len);

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size);

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);

void assert_int16_array(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size);

void assert_byte_array(const int8_t expected[], size_t expected_size, int8_t *actual, size_t actual_size);

void assert_files(FILE *expected, size_t expected_total, uint8_t *expected_buffer, uint8_t *actual_buffer, size_t batch, FILE *actual);

int read_data(uint8_t *output, size_t *output_len, size_t len, FILE *file);
#endif //SDR_MODEM_UTILS_H
