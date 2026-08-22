#include <stdlib.h>
#include <unity.h>
#include <math.h>
#include "../src/app_config.h"

app_config *config = NULL;

void test_missing_file() {
  char *argv[] = {"test_app_config", "--config", "non-existing-configuration-file.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_format() {
  char *argv[] = {"test_app_config", "--config", "invalid.format.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_timeout() {
  char *argv[] = {"test_app_config", "--config", "invalid.timeout.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_tx_sdr_type() {
  char *argv[] = {"test_app_config", "--config", "invalid.tx_sdr_type.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_rx_sdr_type() {
  char *argv[] = {"test_app_config", "--config", "invalid.rx_sdr_type.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_minimal_config() {
  char *argv[] = {"test_app_config", "--config", "minimal.conf", "--rx_sdr_type", "none", NULL};
  int code = app_config_create(5, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
}

void test_pluto_enabled() {
  char *argv[] = {"test_app_config", "--config", "pluto_enabled.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_PLUTOSDR, config->tx_sdr_type);
  TEST_ASSERT(config->iio != NULL);
  TEST_ASSERT(fabsl(10.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(20000, config->tx_plutosdr_timeout_millis);
}

void test_success() {
  char *argv[] = {"test_app_config", "--config", "full.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config->bind_address);
  TEST_ASSERT_EQUAL_INT(8091, config->port);
  TEST_ASSERT_EQUAL_INT(10, config->read_timeout_seconds);
  TEST_ASSERT_EQUAL_INT(2048, config->buffer_size);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_SDR_SERVER, config->rx_sdr_type);
  TEST_ASSERT_EQUAL_INT(64, config->queue_size);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_NONE, config->tx_sdr_type);
  TEST_ASSERT(config->iio == NULL);
  TEST_ASSERT(fabsl(0.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(0, config->tx_plutosdr_timeout_millis);
}

void test_override_from_cli() {
  char *argv[] = {"test_app_config", "--config", "full.conf", "--bind_address", "192.168.1.1", NULL};
  int code = app_config_create(5, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("192.168.1.1", config->bind_address);
}

void test_override_with_invalid() {
  char *argv[] = {"test_app_config", "--config", "full.conf", "--bind_address", NULL};
  int code = app_config_create(4, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config->bind_address);
}

void test_invalid() {
  char *argv1[] = {"test_app_config", "--rx_sdr_type", "file", "--bind_address", "127.0.0.1", NULL};
  int code = app_config_create(5, argv1, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);

  char *argv2[] = {"test_app_config", "--rx_sdr_type", "file", NULL};
  code = app_config_create(3, argv2, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_merge_gfsk_settings() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--rx_sdr_type", "file", "--rx_file", "/non-existing-directory/in.cf32", "--output", "/some-path", "--tx_sdr_type", "none", "--rx_gfsk_baud_rate", "4800", NULL};
  int code = app_config_create(13, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(4800, config->rx_req.gfsk->baud_rate);
}

void tearDown() {
  app_config_destroy(config);
  config = NULL;
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success);
  RUN_TEST(test_minimal_config);
  RUN_TEST(test_invalid_timeout);
  RUN_TEST(test_invalid_format);
  RUN_TEST(test_missing_file);
  RUN_TEST(test_unknown_tx_sdr_type);
  RUN_TEST(test_unknown_rx_sdr_type);
  RUN_TEST(test_pluto_enabled);
  RUN_TEST(test_override_from_cli);
  RUN_TEST(test_override_with_invalid);
  RUN_TEST(test_invalid);
  RUN_TEST(test_merge_gfsk_settings);
  return UNITY_END();
}
