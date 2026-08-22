#define _POSIX_C_SOURCE 200809L

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
  char *argv[] = {"test_cli", "--config", "cli.conf", "--rx_sdr_type", "file", "--rx_file", "/non-existing-directory/in.cf32", "--output", rx_output_path, "--tx_sdr_type", "none", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_fails_missing_output_directory() {
  char *argv[] = {"test_cli", "--config", "cli.conf", "--rx_sdr_type", "file", "--rx_file", "inputnan.cf32", "--output", "/non-existing-directory/out.s8", "--tx_sdr_type", "none", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_fails_missing_input_file() {
  char *argv[] = {"test_cli", "--config", "cli.conf", "--tx_sdr_type", "file", "--tx_file", tx_output_path, "--input", "/non-existing-directory/in.bin", "--rx_sdr_type", "none", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_tx_modulate_to_file() {
  const uint8_t data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  write_bytes(tx_input_path, data, sizeof(data));

  char *argv[] = {"test_cli", "--config", "cli.conf", "--tx_sdr_type", "file", "--tx_file", tx_output_path, "--input", tx_input_path, "--rx_sdr_type", "none", "--buffer_size", "10", NULL};
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
  char *argv[] = {"test_cli", "--config", "cli.conf", "--rx_sdr_type", "file", "--rx_file", "lucky7.cf32", "--output", rx_output_path, "--tx_sdr_type", "none", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = cli_create(config, &cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = cli_process(cli_instance);
  TEST_ASSERT_EQUAL_INT(0, code);
  // make sure output is flushed to the output file
  cli_destroy(cli_instance);
  cli_instance = NULL;
  assert_s8_files("expected_cli_rx_output.s8", rx_output_path, 1000, 0);
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
