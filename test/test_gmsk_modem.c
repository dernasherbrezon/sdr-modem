#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/gmsk_modem.h"
#include "utils.h"

gmsk_modem *demod = NULL;
uint8_t *buffer = NULL;
FILE *input = NULL;
FILE *expected = NULL;
float *result = NULL;
uint8_t *mod_input = NULL;

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
    gmsk_modem_demodulate((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
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

void setup_byte_data(uint8_t **input, size_t input_offset, size_t len) {
  uint8_t *result = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (uint8_t) (input_offset + i);
  }
  *input = result;
}

void test_convolve() {
  float x[3] = {0, 1, 0.5F};
  float y[3] = {1, 2, 3};
  size_t result_len = 0;
  int code = gmsk_modem_convolve(x, 3, y, 3, &result, &result_len);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float expected[5] = {0, 1, 2.5F, 4, 1.5F};
  assert_float_array(expected, 5, result, result_len);
}

void test_exceeded_input() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, 10, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_byte_data(&mod_input, 0, 11);

  float complex *output = NULL;
  size_t output_len = 0;
  gmsk_modem_modulate(mod_input, 11, &output, &output_len, demod);
  TEST_ASSERT_EQUAL_INT(0, output_len);
}

void test_modulation() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, 1000, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_byte_data(&mod_input, 0, 10);

  float complex *output = NULL;
  size_t output_len = 0;
  gmsk_modem_modulate(mod_input, 10, &output, &output_len, demod);

  // 10 * 2 (samples per symbol) * 8 (bit) * 2 (complex) = 320
  const float expected_output[320] = {
    1.000000F, -0.000000F, 1.000000F, -0.000594F, 0.992216F, -0.124531F, 0.555077F, -0.831799F, -0.382684F, -0.923880F, -0.980785F, -0.195090F, -0.707107F, 0.707107F, 0.195090F, 0.980785F, 0.923880F, 0.382683F, 0.831470F, -0.555570F, 0.000000F, -1.000000F, -0.831470F, -0.555570F, -0.923880F,
    0.382683F, -0.195090F, 0.980785F, 0.707107F, 0.707107F, 0.980785F, -0.195090F, 0.382684F, -0.923879F, -0.555570F, -0.831470F, -1.000000F, -0.000000F, -0.555570F, 0.831469F, 0.382683F, 0.923880F, 0.980785F, 0.195091F, 0.707107F, -0.707106F, -0.195090F, -0.980785F, -0.923879F, -0.382684F,
    -0.831470F, 0.555570F, -0.000000F, 1.000000F, 0.831469F, 0.555571F, 0.923880F, -0.382683F, 0.195091F, -0.980785F, -0.707106F, -0.707107F, -0.981016F, 0.193925F, -0.599126F, 0.800655F, -0.980785F, 0.195090F, -0.859917F, -0.510434F, -0.980553F, 0.196254F, -0.382684F, 0.923879F, 0.555570F,
    0.831470F, 1.000000F, 0.000001F, 0.555571F, -0.831469F, -0.382682F, -0.923880F, -0.980785F, -0.195091F, -0.707107F, 0.707106F, 0.195089F, 0.980785F, 0.923879F, 0.382684F, 0.832129F, -0.554582F, 0.247123F, -0.968984F, 0.831470F, -0.555569F, 0.989794F, 0.142504F, 0.830810F, -0.556556F, 0.000001F,
    -1.000000F, -0.831469F, -0.555571F, -0.923880F, 0.382683F, -0.195091F, 0.980785F, 0.707106F, 0.707108F, 0.980786F, -0.195089F, 0.382685F, -0.923879F, -0.555569F, -0.831470F, -1.000000F, -0.000001F, -0.555571F, 0.831469F, 0.382682F, 0.923880F, 0.980553F, 0.196256F, 0.859918F, -0.510432F,
    0.980553F, 0.196255F, 0.382682F, 0.923880F, -0.554584F, 0.832128F, -0.968985F, 0.247121F, -0.554584F, 0.832128F, 0.382682F, 0.923880F, 0.980785F, 0.195092F, 0.707108F, -0.707106F, -0.195089F, -0.980785F, -0.923879F, -0.382685F, -0.831470F, 0.555569F, -0.000001F, 1.000000F, 0.830809F, 0.556558F,
    0.989795F, -0.142501F, 0.831469F, 0.555571F, 0.247121F, 0.968985F, 0.832128F, 0.554584F, 0.923880F, -0.382682F, 0.195092F, -0.980785F, -0.707106F, -0.707108F, -0.980786F, 0.195089F, -0.382684F, 0.923879F, 0.555569F, 0.831470F, 1.000000F, 0.000001F, 0.555571F, -0.831469F, -0.382682F, -0.923880F,
    -0.980785F, -0.195092F, -0.707108F, 0.707106F, 0.193925F, 0.981016F, 0.800654F, 0.599126F, 0.195089F, 0.980786F, -0.510435F, 0.859917F, 0.195089F, 0.980786F, 0.800654F, 0.599126F, 0.195089F, 0.980786F, -0.510435F, 0.859917F, 0.196253F, 0.980553F, 0.923879F, 0.382685F, 0.831471F, -0.555569F,
    0.000002F, -1.000000F, -0.831469F, -0.555572F, -0.923880F, 0.382682F, -0.195092F, 0.980785F, 0.707106F, 0.707108F, 0.981016F, -0.193924F, 0.599127F, -0.800654F, 0.981016F, -0.193924F, 0.707106F, 0.707108F, -0.193928F, 0.981016F, -0.800656F, 0.599124F, -0.193928F, 0.981016F, 0.707106F, 0.707108F,
    0.980786F, -0.195089F, 0.382685F, -0.923879F, -0.555569F, -0.831470F, -1.000000F, -0.000002F, -0.555572F, 0.831469F, 0.382682F, 0.923880F, 0.980785F, 0.195092F, 0.707108F, -0.707105F, -0.193924F, -0.981017F, -0.800654F, -0.599127F, -0.193924F, -0.981017F, 0.707108F, -0.707105F, 0.980785F,
    0.195092F, 0.382682F, 0.923880F, -0.554584F, 0.832128F, -0.968985F, 0.247121F, -0.554584F, 0.832128F, 0.382682F, 0.923880F, 0.980785F, 0.195092F, 0.707108F, -0.707106F, -0.195089F, -0.980786F, -0.923879F, -0.382685F, -0.832129F, 0.554582F, -0.247124F, 0.968984F, -0.831470F, 0.555569F,
    -0.989794F, -0.142504F, -0.830810F, 0.556556F, -0.000001F, 1.000000F, 0.831469F, 0.555571F, 0.923880F, -0.382682F, 0.195092F, -0.980785F, -0.707106F, -0.707108F, -0.980786F, 0.195089F, -0.382685F, 0.923879F, 0.555569F, 0.831471F, 1.000000F, 0.000002F, 0.555572F, -0.831469F, -0.382682F,
    -0.923880F, -0.980553F, -0.196256F, -0.859918F, 0.510432F, -0.980785F, -0.195092F, -0.599124F, -0.800656F, -0.981016F, -0.193928F, -0.707108F, 0.707106F, 0.193924F, 0.981016F
  };

  assert_complex_array(expected_output, 160, output, output_len);
}

void test_demodulation() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("nusat.cf32", "processed.s8");
}

void test_nan() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("inputnan.cf32", "nan.s8");
}

void test_handle_lucky7() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.s8");
}

void test_no_dc() {
  GmskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gmsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.nodc.s8");
}

void tearDown() {
  if (demod != NULL) {
    gmsk_modem_destroy(demod);
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
  RUN_TEST(test_demodulation);
  RUN_TEST(test_nan);
  RUN_TEST(test_handle_lucky7);
  RUN_TEST(test_no_dc);
  RUN_TEST(test_convolve);
  RUN_TEST(test_modulation);
  RUN_TEST(test_exceeded_input);
  return UNITY_END();
}
