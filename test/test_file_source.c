#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
#include "../src/sdr/file_source.h"
#include "utils.h"

const char *tmp_folder;
sdr_device *device = NULL;
char filename[4096];
char gz_filename[4096];

void test_rx_invalid_arguments() {
  int max_output_buffer_length = 2000;
  int code = file_source_create(1, "/non-existing-file", FILE_FORMAT_CF32, NULL, FILE_FORMAT_CF32, 48000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = file_source_create(1, filename, FILE_FORMAT_CF32, NULL, FILE_FORMAT_CF32, 48000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_tx_invalid_arguments() {
  int max_output_buffer_length = 2000;
  int code = file_source_create(1, NULL, FILE_FORMAT_CF32, "/", FILE_FORMAT_CF32, 48000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = file_source_create(1, NULL, FILE_FORMAT_CF32, filename, FILE_FORMAT_CF32, 48000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, max_output_buffer_length + 1, device->plugin);
  TEST_ASSERT_EQUAL_INT(-1, code);

  complex float *output = NULL;
  size_t output_len = 0;
  code = device->sdr_process_rx(&output, &output_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_success() {
  int code = file_source_create(1, NULL, FILE_FORMAT_CF32, filename, FILE_FORMAT_CF32, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(0, code);
  device->destroy(device->plugin);
  free(device);
  device = NULL;

  code = file_source_create(1, filename, FILE_FORMAT_CF32, NULL, FILE_FORMAT_CF32, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);
  complex float *output = NULL;
  size_t output_len = 0;
  device->sdr_process_rx(&output, &output_len, device->plugin);
  assert_complex_array(buffer, buffer_len, output, output_len);
}

void test_gz_success() {
  int code = file_source_create(1, NULL, FILE_FORMAT_CF32, gz_filename, FILE_FORMAT_CF32, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(0, code);
  device->destroy(device->plugin);
  free(device);
  device = NULL;

  code = file_source_create(1, gz_filename, FILE_FORMAT_CF32, NULL, FILE_FORMAT_CF32, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);
  complex float *output = NULL;
  size_t output_len = 0;
  device->sdr_process_rx(&output, &output_len, device->plugin);
  assert_complex_array(buffer, buffer_len, output, output_len);
}

void test_cu8_success() {
  int code = file_source_create(1, NULL, FILE_FORMAT_CF32, filename, FILE_FORMAT_CU8, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {-1.0f, -0.5f, 0.0f, 0.25f, 0.5f, -0.75f, 1.0f, 0.1f, -0.1f, 0.9f};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(0, code);
  device->destroy(device->plugin);
  free(device);
  device = NULL;

  code = file_source_create(1, filename, FILE_FORMAT_CU8, NULL, FILE_FORMAT_CF32, 48000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);
  complex float *output = NULL;
  size_t output_len = 0;
  device->sdr_process_rx(&output, &output_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(buffer_len, output_len);
  for (size_t i = 0; i < buffer_len; i++) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, buffer[2 * i], crealf(output[i]));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, buffer[2 * i + 1], cimagf(output[i]));
  }
}

void tearDown() {
  if (device != NULL) {
    device->destroy(device->plugin);
    free(device);
    device = NULL;
  }
}

void setUp() {
  tmp_folder = getenv("TMPDIR");
  if (tmp_folder == NULL) {
    tmp_folder = "/tmp";
  }
  snprintf(filename, sizeof(filename), "%s/tx.cf32", tmp_folder);
  snprintf(gz_filename, sizeof(gz_filename), "%s/tx.cf32.gz", tmp_folder);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success);
  RUN_TEST(test_gz_success);
  RUN_TEST(test_cu8_success);
  RUN_TEST(test_tx_invalid_arguments);
  RUN_TEST(test_rx_invalid_arguments);
  return UNITY_END();
}
