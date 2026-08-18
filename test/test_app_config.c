#include <stdlib.h>
#include <unity.h>
#include <math.h>
#include "../src/app_config.h"

app_config *config = NULL;

void test_missing_file() {
  int code = app_config_create("non-existing-configuration-file.conf", &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_format() {
  int code = app_config_create("invalid.format.conf", &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_timeout() {
  int code = app_config_create("invalid.timeout.conf", &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_tx_sdr_type() {
  int code = app_config_create("invalid.tx_sdr_type.conf", &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_rx_sdr_type() {
  int code = app_config_create("invalid.rx_sdr_type.conf", &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_minimal_config() {
  int code = app_config_create("minimal.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
}

void test_pluto_enabled() {
  int code = app_config_create("pluto_enabled.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(TX_SDR_TYPE_PLUTOSDR, config->tx_sdr_type);
  TEST_ASSERT(config->iio != NULL);
  TEST_ASSERT(fabsl(10.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(20000, config->tx_plutosdr_timeout_millis);
}

void test_success() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config->bind_address);
  TEST_ASSERT_EQUAL_INT(8091, config->port);
  TEST_ASSERT_EQUAL_INT(10, config->read_timeout_seconds);
  TEST_ASSERT_EQUAL_INT(2048, config->buffer_size);
  TEST_ASSERT_EQUAL_INT(RX_SDR_TYPE_SDR_SERVER, config->rx_sdr_type);
  TEST_ASSERT_EQUAL_INT(64, config->queue_size);
  TEST_ASSERT_EQUAL_INT(TX_SDR_TYPE_NONE, config->tx_sdr_type);
  TEST_ASSERT(config->iio == NULL);
  TEST_ASSERT(fabsl(0.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(10000, config->tx_plutosdr_timeout_millis);
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
  return UNITY_END();
}
