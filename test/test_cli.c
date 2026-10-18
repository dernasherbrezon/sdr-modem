#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unity.h>

#include "../src/cli.h"
#include "../src/app_config.h"
#include "../src/dsp/gfsk_modem.h"
#include "utils.h"

const char *tmp_folder;
char rx_output_path[4096];
char tx_output_path[4096];
char tx_input_path[4096];

app_config *config = NULL;
cli *cli_instance = NULL;
FILE *actual_file = NULL;
FILE *expected_file = NULL;
uint8_t *actual_buffer = NULL;
uint8_t *expected_buffer = NULL;

void write_bytes(const char *path, const uint8_t *data, size_t len) {
  FILE *f = fopen(path, "wb");
  TEST_ASSERT(f != NULL);
  size_t written = fwrite(data, sizeof(uint8_t), len, f);
  TEST_ASSERT_EQUAL_INT(len, written);
  fclose(f);
}

void test_create_fails_missing_rx_file() {
  char *argv[] = {"test_cli", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "/non-existing-directory/in.cf32", "--output", rx_output_path, NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_fails_missing_output_directory() {
  char *argv[] = {"test_cli", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "inputnan.cf32", "--output", "/non-existing-directory/out.s8", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_fails_missing_input_file() {
  char *argv[] = {"test_cli", "--config", "cli.conf", "--direction", "tx", "--sdr_type", "file", "--tx_file", tx_output_path, "--input", "/non-existing-directory/in.bin", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_tx_modulate_to_file() {
  const uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  write_bytes(tx_input_path, data, sizeof(data));

  char *argv[] = {"test_cli", "--config", "cli.conf", "--direction", "tx", "--sdr_type", "file", "--tx_file", tx_output_path, "--input", tx_input_path, "--buffer_size", "10", NULL};
  int code = app_config_create(13, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_process(cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  // make sure output is flushed to the output file
  cli_destroy(cli_instance);
  cli_instance = NULL;
  assert_cf32_files("expected_cli_tx_output.cf32", tx_output_path, 100, 0.001f);
}

void test_rx_demodulate_to_file() {
  // tx a known burst: syncword (from cli.conf) + payload, padded with zeros so that the correlator can flush it
  const uint8_t syncword[] = {0xCC, 0xCC, 0xCC, 0xFE};
  const uint8_t payload[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x0F, 0xA5, 0x5A, 0xC3};
  uint8_t frame[64] = {0};
  memcpy(frame + 4, syncword, sizeof(syncword));
  memcpy(frame + 4 + sizeof(syncword), payload, sizeof(payload));
  write_bytes(tx_input_path, frame, sizeof(frame));

  char *tx_argv[] = {"test_cli", "--config", "cli.conf", "--direction", "tx", "--sdr_type", "file", "--tx_file", tx_output_path, "--input", tx_input_path, "--buffer_size", "64", NULL};
  int code = app_config_create(13, tx_argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_process(cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  cli_destroy(cli_instance);
  cli_instance = NULL;
  app_config_destroy(config);
  config = NULL;

  // rx the burst back
  char *rx_argv[] = {"test_cli", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", tx_output_path, "--output", rx_output_path, NULL};
  code = app_config_create(11, rx_argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_process(cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  // make sure output is flushed to the output file
  cli_destroy(cli_instance);
  cli_instance = NULL;

  // soft bits are noisy and platform dependent, so compare only the signs of the bits: the sync word followed by the payload
  FILE *f = fopen(rx_output_path, "rb");
  TEST_ASSERT(f != NULL);
  int8_t soft[4096];
  size_t soft_len = fread(soft, sizeof(int8_t), sizeof(soft), f);
  fclose(f);
  const size_t expected_bits = (sizeof(syncword) + sizeof(payload)) * 8;
  TEST_ASSERT(soft_len >= expected_bits);
  size_t found = soft_len;
  for (size_t start = 0; start + expected_bits <= soft_len && found == soft_len; start++) {
    size_t i = 0;
    for (; i < expected_bits; i++) {
      uint8_t byte = i < sizeof(syncword) * 8 ? syncword[i / 8] : payload[i / 8 - sizeof(syncword)];
      int expected_bit = (byte >> (7 - i % 8)) & 1;
      if ((soft[start + i] > 0) != expected_bit) {
        break;
      }
    }
    if (i == expected_bits) {
      found = start;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found != soft_len, "syncword and payload were not demodulated");
}

void tearDown() {
  if (cli_instance != NULL) {
    cli_destroy(cli_instance);
    cli_instance = NULL;
  }
  if (config != NULL) {
    app_config_destroy(config);
    config = NULL;
  }
  if (actual_file != NULL) {
    fclose(actual_file);
    actual_file = NULL;
  }
  if (expected_file != NULL) {
    fclose(expected_file);
    expected_file = NULL;
  }
  if (actual_buffer != NULL) {
    free(actual_buffer);
    actual_buffer = NULL;
  }
  if (expected_buffer != NULL) {
    free(expected_buffer);
    expected_buffer = NULL;
  }
}

void setUp() {
  tmp_folder = getenv("TMPDIR");
  if (tmp_folder == NULL) {
    tmp_folder = "/tmp";
  }
  snprintf(rx_output_path, sizeof(rx_output_path), "%s/test_cli_rx_output.s8", tmp_folder);
  snprintf(tx_output_path, sizeof(tx_output_path), "%s/test_cli_tx_output.cf32", tmp_folder);
  snprintf(tx_input_path, sizeof(tx_input_path), "%s/test_cli_tx_input.bin", tmp_folder);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_fails_missing_rx_file);
  RUN_TEST(test_create_fails_missing_output_directory);
  RUN_TEST(test_create_fails_missing_input_file);
  RUN_TEST(test_tx_modulate_to_file);
  RUN_TEST(test_rx_demodulate_to_file);
  return UNITY_END();
}
