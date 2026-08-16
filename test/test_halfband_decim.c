#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unity.h>
#include "../src/dsp/halfband_decim.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

halfband_decim *filter = NULL;
float complex *input = NULL;

void test_decimation_across_calls() {
  int code = halfband_decim_create(2, 0.4f, 60.0f, 64, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_input_complex_data(&input, 0, 40);

  float complex *output = NULL;
  size_t output_len = 0;
  // decimation is 4. first call leaves 1 sample of history for the next call
  halfband_decim_process(input, 9, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(2, output_len);

  halfband_decim_process(input + 9, 31, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(8, output_len);
}

void test_exceeded_input() {
  int code = halfband_decim_create(1, 0.4f, 60.0f, 10, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_input_complex_data(&input, 0, 11);

  float complex *output = NULL;
  size_t output_len = 0;
  halfband_decim_process(input, 11, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(0, output_len);
}

void test_tone_passthrough() {
  int code = halfband_decim_create(1, 0.4f, 60.0f, 4096, &filter);
  TEST_ASSERT_EQUAL_INT(0, code);

  size_t input_len = 4096;
  input = malloc(sizeof(float complex) * input_len);
  TEST_ASSERT(input != NULL);
  // low frequency tone, well within the half-band passband
  float f = 0.05f;
  for (size_t i = 0; i < input_len; i++) {
    input[i] = cosf(2 * (float) M_PI * f * (float) i) + I * sinf(2 * (float) M_PI * f * (float) i);
  }

  float complex *output = NULL;
  size_t output_len = 0;
  halfband_decim_process(input, input_len, &output, &output_len, filter);
  TEST_ASSERT_EQUAL_INT(2048, output_len);

  // skip the filter's transient response and check the steady-state magnitude is preserved
  float sum_mag = 0;
  size_t count = 0;
  for (size_t i = 200; i < output_len; i++) {
    sum_mag += cabsf(output[i]);
    count++;
  }
  float avg_mag = sum_mag / (float) count;
  TEST_ASSERT(fabsf(avg_mag - 1.0f) < 0.05f);
}

void tearDown() {
  if (filter != NULL) {
    halfband_decim_destroy(filter);
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
  RUN_TEST(test_decimation_across_calls);
  RUN_TEST(test_exceeded_input);
  RUN_TEST(test_tone_passthrough);
  return UNITY_END();
}
