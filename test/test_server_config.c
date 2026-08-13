#include <stdlib.h>
#include <unity.h>
#include <math.h>
#include "../src/server_config.h"

struct server_config *config = NULL;

void test_missing_file() {
  int code = server_config_create(&config, "non-existing-configuration-file.conf");
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_format() {
  int code = server_config_create(&config, "invalid.format.conf");
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_timeout() {
  int code = server_config_create(&config, "invalid.timeout.conf");
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_tx_sdr_type() {
  int code = server_config_create(&config, "invalid.tx_sdr_type.conf");
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_unknown_rx_sdr_type() {
  int code = server_config_create(&config, "invalid.rx_sdr_type.conf");
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_minimal_config() {
  int code = server_config_create(&config, "minimal.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
}

void test_pluto_enabled() {
  int code = server_config_create(&config, "pluto_enabled.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(TX_SDR_TYPE_PLUTOSDR, config->tx_sdr_type);
  TEST_ASSERT(config->iio != NULL);
  TEST_ASSERT(fabsl(10.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(20000, config->tx_plutosdr_timeout_millis);
}

void test_success() {
  int code = server_config_create(&config, "full.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config->bind_address);
  TEST_ASSERT_EQUAL_INT(8091, config->port);
  TEST_ASSERT_EQUAL_INT(10, config->read_timeout_seconds);
  TEST_ASSERT_EQUAL_INT(2048, config->buffer_size);
  TEST_ASSERT_EQUAL_INT(RX_SDR_TYPE_SDR_SERVER, config->rx_sdr_type);
  TEST_ASSERT_EQUAL_STRING("/tmp/", config->base_path);
  TEST_ASSERT_EQUAL_INT(64, config->queue_size);
  TEST_ASSERT_EQUAL_INT(TX_SDR_TYPE_NONE, config->tx_sdr_type);
  TEST_ASSERT(config->iio == NULL);
  TEST_ASSERT(fabsl(0.0 - config->tx_plutosdr_gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(10000, config->tx_plutosdr_timeout_millis);
}

void tearDown() {
  server_config_destroy(config);
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
