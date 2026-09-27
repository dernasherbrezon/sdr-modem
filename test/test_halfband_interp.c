#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unity.h>
#include "../src/dsp/halfband_interp.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

halfband_interp *filter = NULL;
float complex *input = NULL;

void test_interpolation_across_calls() {
  int code = halfband_interp_create(2, 0.4f, 60.0f, 64, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(256, halfband_interp_max_output_buffer_length(filter));

  setup_input_complex_data(&input, 0, 40);

  float complex *output = NULL;
  size_t output_len = 0;
  // interpolation is 4
  halfband_interp_process(input, 9, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(36, output_len);

  halfband_interp_process(input + 9, 31, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(124, output_len);
}

void test_exceeded_input() {
  int code = halfband_interp_create(1, 0.4f, 60.0f, 10, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_input_complex_data(&input, 0, 11);

  float complex *output = NULL;
  size_t output_len = 0;
  halfband_interp_process(input, 11, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(0, output_len);
  TEST_ASSERT_NULL(output);
}

void test_tone_passthrough() {
  int code = halfband_interp_create(2, 0.4f, 60.0f, 1024, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);

  size_t input_len = 1024;
  input = malloc(sizeof(float complex) * input_len);
  TEST_ASSERT(input != NULL);
  // low frequency tone, well within the half-band passband
  float f = 0.05f;
  for (size_t i = 0; i < input_len; i++) {
    input[i] = cosf(2 * (float) M_PI * f * (float) i) + I * sinf(2 * (float) M_PI * f * (float) i);
  }

  float complex *output = NULL;
  size_t output_len = 0;
  halfband_interp_process(input, input_len, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(4096, output_len);

  // skip the filter's transient response. magnitude is preserved and the tone is at f / 4
  float sum_mag = 0;
  double sum_phase = 0;
  size_t count = 0;
  for (size_t i = 400; i < output_len - 1; i++) {
    sum_mag += cabsf(output[i]);
    sum_phase += cargf(conjf(output[i]) * output[i + 1]);
    count++;
  }
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, sum_mag / (float) count);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, f / 4, (float) (sum_phase / (double) count / (2 * M_PI)));
}

void tearDown() {
  if (filter != NULL) {
    halfband_interp_destroy(filter);
    filter = NULL;
  }
  if (input != NULL) {
    free(input);
    input = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_interpolation_across_calls);
  RUN_TEST(test_exceeded_input);
  RUN_TEST(test_tone_passthrough);
  return UNITY_END();
}
