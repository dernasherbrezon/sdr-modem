#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/fsk_demod.h"
#include "utils.h"

fsk_demod *demod = NULL;
uint8_t *buffer = NULL;
FILE *input = NULL;
FILE *expected = NULL;
// buffer length is important here. do not change it
// all test data was generated and verified with this buffer length
// and VOLK_GENERIC=1
// different buffer length or using SIMD can cause different precision when dealing with floats
// this small precision issue can propagate further and cause small differences in the output.
// i.e. instead of -31, it can produce -30
uint32_t max_buffer_length = 4096;

void assert_files_and_demod(const char *input_filename, const char *expected_filename) {
  input = fopen(input_filename, "rb");
  TEST_ASSERT(input != NULL);
  expected = fopen(expected_filename, "rb");
  TEST_ASSERT(expected != NULL);
  size_t buffer_len = sizeof(float complex) * max_buffer_length;
  buffer = malloc(sizeof(uint8_t) * buffer_len);
  TEST_ASSERT(buffer != NULL);
  size_t j = 0;
  while (true) {
    size_t actual_read = 0;
    int code = read_data(buffer, &actual_read, buffer_len, input);
    if (code != 0 && actual_read == 0) {
      break;
    }
    int8_t *output = NULL;
    size_t output_len = 0;
    fsk_demod_process((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
    code = read_data(buffer, &actual_read, output_len, expected);
    TEST_ASSERT_EQUAL_INT(0, code);
    TEST_ASSERT_EQUAL_INT(output_len, actual_read);
    for (size_t i = 0; i < actual_read; i++) {
      // can't make test working across macbook, raspberrypi and travis
      // all of them have different float-precision issues
      // where results slightly different
      TEST_ASSERT(abs((int8_t) buffer[i] - output[i]) <= 2);
    }
  }
}

void test_normal() {
  int code = fsk_demod_create(192000, 40000, 5000, 1, 2000, true, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("nusat.cf32", "processed.s8");
}

void test_nan() {
  int code = fsk_demod_create(240000, 9600, 5000, 1, 2000, true, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("inputnan.cf32", "nan.s8");
}

void test_handle_lucky7() {
  int code = fsk_demod_create(48000, 4800, 5000, 2, 2000, true, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.s8");
}

void test_no_dc() {
  int code = fsk_demod_create(48000, 4800, 5000, 2, 2000, false, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.nodc.s8");
}

void tearDown() {
  if (demod != NULL) {
    fsk_demod_destroy(demod);
    demod = NULL;
  }
  if (buffer != NULL) {
    free(buffer);
    buffer = NULL;
  }
  if (input != NULL) {
    fclose(input);
    input = NULL;
  }
  if (expected != NULL) {
    fclose(expected);
    expected = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_normal);
  RUN_TEST(test_nan);
  RUN_TEST(test_handle_lucky7);
  RUN_TEST(test_no_dc);
  return UNITY_END();
}
