#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
#include <stdbool.h>
#include "../src/dsp/doppler.h"
#include "utils.h"
#include <time.h>

FILE *input_file = NULL;
uint8_t *input_buffer = NULL;
FILE *expected_file = NULL;
uint8_t *expected_buffer = NULL;
doppler *dopp = NULL;
char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
int max_buffer_length = 2000;

void test_invalid_arguments() {
  int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 0, 1583840449, max_buffer_length, tle, &dopp);
  TEST_ASSERT_EQUAL_INT(0, code);

  float complex *output = NULL;
  size_t output_len = 0;
  doppler_process_rx(NULL, 12, &output, &output_len, dopp);
  TEST_ASSERT(output == NULL);

  const float buffer[2] = {1, 2};
  doppler_process_rx((float complex *) buffer, 0, &output, &output_len, dopp);
  TEST_ASSERT(output == NULL);

  size_t input_buffer_len = max_buffer_length + 1;
  input_buffer = malloc(sizeof(float complex) * input_buffer_len);
  TEST_ASSERT(input_buffer != NULL);
  doppler_process_rx((float complex *) input_buffer, input_buffer_len, &output, &output_len, dopp);
  TEST_ASSERT(output == NULL);
}

void assert_success_rx(int buffer_length, const char *expected_filename) {
  int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 0, 1583840449, buffer_length, tle, &dopp);
  TEST_ASSERT_EQUAL_INT(0, code);

  input_file = fopen("lucky7.cf32", "rb");
  TEST_ASSERT(input_file != NULL);
  expected_file = fopen(expected_filename, "rb");
  TEST_ASSERT(expected_file != NULL);

  input_buffer = malloc(max_buffer_length * sizeof(float complex));
  TEST_ASSERT(input_buffer != NULL);
  expected_buffer = malloc(max_buffer_length * sizeof(float complex));
  TEST_ASSERT(expected_buffer != NULL);
  while (true) {
    size_t actually_read = fread(input_buffer, sizeof(float complex), max_buffer_length, input_file);
    if (actually_read == 0) {
      break;
    }
    float complex *output = NULL;
    size_t output_len = 0;
    doppler_process_rx((float complex *) input_buffer, actually_read, &output, &output_len, dopp);

    size_t actually_expected_read = fread(expected_buffer, sizeof(float complex), actually_read, expected_file);
    TEST_ASSERT_EQUAL_INT(actually_expected_read, actually_read);

    assert_complex_array((const float *) expected_buffer, actually_expected_read, output, output_len);
  }
}

void test_success_rx() {
  assert_success_rx(max_buffer_length, "lucky7.expected.cf32");
}

void test_success_rx_47000() {
  assert_success_rx(47000, "lucky7.expected.47000.cf32");
}

void test_success_rx_95000() {
  assert_success_rx(95000, "lucky7.expected.95000.cf32");
}

void test_success_tx() {
  int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 0, 1583840449, max_buffer_length, tle, &dopp);
  TEST_ASSERT_EQUAL_INT(0, code);

  // use RX inverted input data for test
  input_file = fopen("lucky7.expected.cf32", "rb");
  TEST_ASSERT(input_file != NULL);
  expected_file = fopen("lucky7.cf32", "rb");
  TEST_ASSERT(expected_file != NULL);

  input_buffer = malloc(max_buffer_length * sizeof(float complex));
  TEST_ASSERT(input_buffer != NULL);
  expected_buffer = malloc(max_buffer_length * sizeof(float complex));
  TEST_ASSERT(expected_buffer != NULL);
  while (true) {
    size_t actually_read = fread(input_buffer, sizeof(float complex), max_buffer_length, input_file);
    if (actually_read == 0) {
      break;
    }
    float complex *output = NULL;
    size_t output_len = 0;
    doppler_process_tx((float complex *) input_buffer, actually_read, &output, &output_len, dopp);

    size_t actually_expected_read = fread(expected_buffer, sizeof(float complex), actually_read, expected_file);
    TEST_ASSERT_EQUAL_INT(actually_expected_read, actually_read);

    assert_complex_array((const float *) expected_buffer, actually_expected_read, output, output_len);
  }
}

void tearDown() {
  if (dopp != NULL) {
    doppler_destroy(dopp);
    dopp = NULL;
  }
  if (input_file != NULL) {
    fclose(input_file);
    input_file = NULL;
  }
  if (input_buffer != NULL) {
    free(input_buffer);
    input_buffer = NULL;
  }
  if (expected_file != NULL) {
    fclose(expected_file);
    expected_file = NULL;
  }
  if (expected_buffer != NULL) {
    free(expected_buffer);
    expected_buffer = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success_rx);
  RUN_TEST(test_success_rx_47000);
  RUN_TEST(test_success_rx_95000);
  RUN_TEST(test_success_tx);
  RUN_TEST(test_invalid_arguments);
  return UNITY_END();
}
