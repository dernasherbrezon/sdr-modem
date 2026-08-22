#ifndef SDR_MODEM_UTILS_H
#define SDR_MODEM_UTILS_H

#include <complex.h>
#include <stdlib.h>
#include "../src/api.pb-c.h"
#include <stdio.h>

struct ModemRequest *create_request();

void setup_input_data(float **input, size_t input_offset, size_t len);

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len);

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size);

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);

void assert_int16_array(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size);

void assert_byte_array(const int8_t expected[], size_t expected_size, int8_t *actual, size_t actual_size, int tolerance);

void assert_s8_files(const char *expected, const char *actual, size_t number_of_items_to_compare, int tolerance);

void assert_cf32_files(const char *expected, const char *actual, size_t number_of_items_to_compare, float tolerance);

int read_data(uint8_t *output, size_t *output_len, size_t len, FILE *file);

char *utils_read_and_copy_str(const char *value);

char ** utils_allocate_tle(char tle[3][80]);

#endif //SDR_MODEM_UTILS_H
