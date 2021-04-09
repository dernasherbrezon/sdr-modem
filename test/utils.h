#ifndef SDR_MODEM_UTILS_H
#define SDR_MODEM_UTILS_H

#include <complex.h>
#include <stdlib.h>

void setup_input_data(float **input, size_t input_offset, size_t len);

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len);

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size);

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size);

#endif //SDR_MODEM_UTILS_H
