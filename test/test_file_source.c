#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
#include "../src/sdr/file_source.h"
#include "utils.h"

const char *tmp_folder;
sdr_device *device = NULL;
char filename[4096];

void test_rx_invalid_arguments() {
  int max_output_buffer_length = 2000;
  int code = file_source_create(1, "/non-existing-file", NULL, 48000, 1000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = file_source_create(1, filename, NULL, 48000, 1000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_tx_invalid_arguments() {
  int max_output_buffer_length = 2000;
  int code = file_source_create(1, NULL, "/", 48000, 1000, max_output_buffer_length, &device);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = file_source_create(1, NULL, filename, 48000, 1000, max_output_buffer_length, &device);
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

void test_rx_offset() {
  int code = file_source_create(1, "tx.cf32", NULL, 48000, 1000, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  complex float *output = NULL;
  size_t output_len = 0;
  code = device->sdr_process_rx(&output, &output_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float expected[10] = {1.000000F, 2.000000F, 2.461137F, 4.352334F, 3.262207F, 7.096337F, 3.405689F, 10.069820F, 2.821140F, 13.154511F};
  size_t expected_len = sizeof(expected) / sizeof(float) / 2;
  assert_complex_array(expected, expected_len, output, output_len);
}

void test_success() {
  int code = file_source_create(1, NULL, filename, 48000, 0, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
  code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
  TEST_ASSERT_EQUAL_INT(0, code);
  device->destroy(device->plugin);
  free(device);
  device = NULL;

  code = file_source_create(1, filename, NULL, 48000, 0, 2000, &device);
  TEST_ASSERT_EQUAL_INT(0, code);
  complex float *output = NULL;
  size_t output_len = 0;
  device->sdr_process_rx(&output, &output_len, device->plugin);
  assert_complex_array(buffer, buffer_len, output, output_len);
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
}

int main(void) {
  UNITY_BEGIN();
  // RUN_TEST(test_success);
  RUN_TEST(test_rx_offset);
  // RUN_TEST(test_tx_invalid_arguments);
  // RUN_TEST(test_rx_invalid_arguments);
  return UNITY_END();
}
