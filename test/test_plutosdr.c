#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unity.h>
#include "../src/sdr/plutosdr.h"
#include "iio_lib_mock.h"
#include "utils.h"

#define BUFFER_LEN 8

sdr_device *device = NULL;
iio_lib *lib = NULL;

static int create_device(uint64_t sample_rate, iio_gain_mode gain_mode, int16_t *rx, size_t rx_len, int16_t *tx) {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(rx, rx_len, tx, &lib));
  plutosdr_settings settings = {
      .tx_powerdown = false,
      .gain_control_mode = gain_mode,
      .timeout_ms = 1000,
      .uri = (char *) "ip:pluto.local",
      .sample_rate = sample_rate,
  };
  return plutosdr_create(&settings, BUFFER_LEN, lib, &device);
}

static sdr_channel_config config(uint64_t frequency, uint64_t sample_rate, float gain) {
  return (sdr_channel_config) {.frequency = frequency, .sample_rate = sample_rate, .gain = gain};
}

void test_create_default_uri() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  // uri is NULL: first context from the scan is used
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NOT_NULL(device);
  TEST_ASSERT_EQUAL_STRING("manual", iio_lib_mock_get_stats()->gain_control_mode);
}

void test_create_default_calib_mode() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  TEST_ASSERT_EQUAL_STRING("manual_tx_quad", iio_lib_mock_get_stats()->calib_mode);
}

void test_create_calib_mode() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local", .calib_mode = (char *) "auto"};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_EQUAL_STRING("auto", iio_lib_mock_get_stats()->calib_mode);
}

void test_create_empty_calib_mode() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local", .calib_mode = (char *) ""};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_EQUAL_STRING("manual_tx_quad", iio_lib_mock_get_stats()->calib_mode);
}

void test_create_calib_mode_before_sample_rate() {
  TEST_ASSERT_EQUAL_INT(0, create_device(3000000, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->calib_mode_sampling_frequency_writes);
  TEST_ASSERT_TRUE(stats->sampling_frequency_writes > 0);
}

void test_create_calib_mode_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  iio_lib_mock_fail_write("calib_mode");
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local"};
  TEST_ASSERT_TRUE(plutosdr_create(&settings, BUFFER_LEN, lib, &device) < 0);
  TEST_ASSERT_NULL(device);
}

void test_create_gain_modes() {
  struct {
    iio_gain_mode mode;
    const char *expected;
  } cases[] = {
      {IIO_GAIN_MODE_MANUAL, "manual"},
      {IIO_GAIN_MODE_FAST_ATTACK, "fast_attack"},
      {IIO_GAIN_MODE_SLOW_ATTACK, "slow_attack"},
      {IIO_GAIN_MODE_HYBRID, "hybrid"},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    TEST_ASSERT_EQUAL_INT(0, create_device(0, cases[i].mode, NULL, 0, NULL));
    TEST_ASSERT_EQUAL_STRING(cases[i].expected, iio_lib_mock_get_stats()->gain_control_mode);
    device->destroy(device->plugin);
    free(device);
    device = NULL;
    iio_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_create_invalid_gain_mode() {
  TEST_ASSERT_EQUAL_INT(-1, create_device(0, (iio_gain_mode) 100, NULL, 0, NULL));
  TEST_ASSERT_NULL(device);
}

void test_create_gain_mode_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  iio_lib_mock_fail_write("gain_control_mode");
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local"};
  TEST_ASSERT_TRUE(plutosdr_create(&settings, BUFFER_LEN, lib, &device) < 0);
  TEST_ASSERT_NULL(device);
}

void test_create_invalid_range_format() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  iio_lib_mock_set_attr(false, "hardwaregain_available", "-1 1 73");
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local"};
  TEST_ASSERT_EQUAL_INT(-EINVAL, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
}

void test_create_default_sample_rate_without_fir() {
  TEST_ASSERT_EQUAL_INT(0, create_device(3000000, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->fir_config_writes);
  TEST_ASSERT_EQUAL_INT(1, stats->fir_enable_calls);
  TEST_ASSERT_FALSE(stats->fir_enabled);
  TEST_ASSERT_EQUAL_INT64(3000000, stats->sampling_frequency[0]);
  TEST_ASSERT_EQUAL_INT64(3000000, stats->rf_bandwidth[0]);
}

void test_create_default_sample_rate_with_fir() {
  TEST_ASSERT_EQUAL_INT(0, create_device(600000, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(1, stats->fir_config_writes);
  TEST_ASSERT_NOT_NULL(strstr(stats->fir_config, "RX 3 GAIN -6 DEC 4\n"));
  TEST_ASSERT_NOT_NULL(strstr(stats->fir_config, "TX 3 GAIN 0 INT 4\n"));
  TEST_ASSERT_TRUE(stats->fir_enabled);
  TEST_ASSERT_EQUAL_INT64(600000, stats->sampling_frequency[0]);
}

void test_create_default_sample_rate_too_low() {
  TEST_ASSERT_EQUAL_INT(-1, create_device(100000, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->sampling_frequency_writes);
}

void test_create_default_sample_rate_same_as_current() {
  iio_lib_mock_create(NULL, 0, NULL, &lib);
  iio_lib_mock_set_current_sample_rate(2400000);
  plutosdr_settings settings = {.gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local", .sample_rate = 2400000};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->sampling_frequency_writes);
}

void test_rx_sample_rate_without_fir() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 3000000, 30);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT64(433000000, stats->frequency);
  TEST_ASSERT_EQUAL_INT64(3000000, stats->sampling_frequency[0]);
  TEST_ASSERT_EQUAL_INT(0, stats->fir_config_writes);
  TEST_ASSERT_FALSE(stats->fir_enabled);
  TEST_ASSERT_EQUAL_INT(1, stats->hardwaregain_writes[0]);
  TEST_ASSERT_EQUAL_FLOAT(30, stats->hardwaregain[0]);
}

void test_rx_sample_rate_boundaries() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  struct {
    uint64_t rate;
    const char *dec;
    bool fir;
  } cases[] = {
      {520834, "DEC 4", true},   // lowest supported
      {1041666, "DEC 4", true},  // just below the DEC 2 boundary
      {1041667, "DEC 2", true},
      {2083333, "DEC 2", true},  // just below no-fir boundary
      {2083334, NULL, false},
      {61440000, NULL, false},   // highest supported
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    sdr_channel_config cfg = config(433000000, cases[i].rate, 0);
    TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
    const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
    TEST_ASSERT_EQUAL_INT64(cases[i].rate, stats->sampling_frequency[0]);
    TEST_ASSERT_EQUAL(cases[i].fir, stats->fir_enabled);
    if (cases[i].dec != NULL) {
      TEST_ASSERT_NOT_NULL(strstr(stats->fir_config, cases[i].dec));
    }
  }
}

void test_rx_sample_rate_down_to_fir_and_back_up() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 600000, 0);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_TRUE(iio_lib_mock_get_stats()->fir_enabled);
  cfg.sample_rate = 3000000;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_FALSE(iio_lib_mock_get_stats()->fir_enabled);
  TEST_ASSERT_EQUAL_INT64(3000000, iio_lib_mock_get_stats()->sampling_frequency[0]);
}

void test_rx_same_sample_rate_not_rewritten() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 3000000, 0);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, iio_lib_mock_get_stats()->sampling_frequency_writes);
  cfg.frequency = 434000000;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, iio_lib_mock_get_stats()->sampling_frequency_writes);
  TEST_ASSERT_EQUAL_INT64(434000000, iio_lib_mock_get_stats()->frequency);
}

void test_rx_invalid_frequency() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(69999999, 3000000, 10);
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.frequency = 0;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.frequency = 6000000001ULL;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  // nothing must reach the device
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->frequency_writes);
  cfg.frequency = 70000000;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
}

void test_rx_invalid_sample_rate() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 520833, 10);
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.sample_rate = 61440001;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->sampling_frequency_writes);
}

void test_rx_invalid_gain() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 3000000, -1.5f);
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.gain = 73.5f;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->hardwaregain_writes[0]);
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->frequency_writes);
  // range edges are valid
  cfg.gain = -1;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  cfg.gain = 73;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_FLOAT(73, iio_lib_mock_get_stats()->hardwaregain[0]);
}

void test_rx_gain_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  iio_lib_mock_fail_write("hardwaregain");
  sdr_channel_config cfg = config(433000000, 3000000, 10);
  TEST_ASSERT_TRUE(device->set_rx_parameters(&cfg, device->plugin) < 0);
}

void test_rx_frequency_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  iio_lib_mock_fail_write("frequency");
  sdr_channel_config cfg = config(433000000, 3000000, 10);
  TEST_ASSERT_TRUE(device->set_rx_parameters(&cfg, device->plugin) < 0);
}

void test_rx_automatic_gain_mode() {
  iio_gain_mode modes[] = {IIO_GAIN_MODE_FAST_ATTACK, IIO_GAIN_MODE_SLOW_ATTACK, IIO_GAIN_MODE_HYBRID};
  for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
    TEST_ASSERT_EQUAL_INT(0, create_device(0, modes[i], NULL, 0, NULL));
    sdr_channel_config cfg = config(433000000, 3000000, 30);
    TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
    // gain is controlled by the AGC
    TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->hardwaregain_writes[0]);
    // but the range is still validated
    cfg.gain = 100;
    TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
    device->destroy(device->plugin);
    free(device);
    device = NULL;
    iio_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_rx_success() {
  // 4 samples: I, Q pairs
  int16_t rx[] = {2048, -2048, 1024, 0, -1024, 512, 0, 2047};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, rx, sizeof(rx) / sizeof(int16_t), NULL));
  sdr_channel_config cfg = config(433000000, 3000000, 30);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  device->start_rx(device->plugin);

  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  const float expected[] = {1.0F, -1.0F, 0.5F, 0.0F, -0.5F, 0.25F, 0.0F, 2047.0F / 2048.0F};
  assert_complex_array(expected, 4, output, output_len);

  device->stop_rx(device->plugin);
}

void test_rx_success_with_fir() {
  int16_t rx[] = {2048, -2048, 1024, 0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, rx, sizeof(rx) / sizeof(int16_t), NULL));
  sdr_channel_config cfg = config(145800000, 600000, 20);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_TRUE(iio_lib_mock_get_stats()->fir_enabled);
  device->start_rx(device->plugin);

  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  const float expected[] = {1.0F, -1.0F, 0.5F, 0.0F};
  assert_complex_array(expected, 2, output, output_len);
  device->stop_rx(device->plugin);
}

void test_rx_not_started() {
  int16_t rx[] = {1, 2};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, rx, 2, NULL));
  float complex *output = (float complex *) 0x1;
  size_t output_len = 100;
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // stopped rx behaves the same
  device->start_rx(device->plugin);
  device->stop_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_buffer_overflow() {
  // 10 samples but the buffer is for 8
  int16_t rx[20] = {0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, rx, 20, NULL));
  device->start_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
  device->stop_rx(device->plugin);
}

void test_rx_restart() {
  int16_t rx[] = {2048, 0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, rx, 2, NULL));
  // start twice must not leak the first buffer
  device->start_rx(device->plugin);
  device->start_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_EQUAL_size_t(1, output_len);
  // stop is idempotent
  device->stop_rx(device->plugin);
  device->stop_rx(device->plugin);
}

void test_tx_success() {
  int16_t tx[40] = {0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, tx));
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT64(433000000, stats->frequency);
  TEST_ASSERT_EQUAL_INT64(3000000, stats->sampling_frequency[1]);
  TEST_ASSERT_EQUAL_INT(1, stats->hardwaregain_writes[1]);
  TEST_ASSERT_EQUAL_FLOAT(-10, stats->hardwaregain[1]);
  TEST_ASSERT_FALSE(stats->fir_enabled);

  // full scale, half scale, clipping on both sides and 0
  float complex input[] = {1.0F + 0.5F * I, -1.0F + 0.0F * I, 2.0F - 2.0F * I};
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_tx(input, 3, device->plugin));
  int16_t *actual = NULL;
  size_t actual_len = 0;
  iio_lib_mock_get_tx(&actual, &actual_len);
  const int16_t expected[] = {32767, 16384, -32768, 0, 32767, -32768};
  assert_int16_array(expected, sizeof(expected) / sizeof(int16_t), actual, actual_len);
}

void test_tx_success_with_fir() {
  int16_t tx[10] = {0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, tx));
  sdr_channel_config cfg = config(433000000, 1500000, -20);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  const iio_lib_mock_stats *stats = iio_lib_mock_get_stats();
  TEST_ASSERT_TRUE(stats->fir_enabled);
  TEST_ASSERT_NOT_NULL(strstr(stats->fir_config, "TX 3 GAIN 0 INT 2\n"));
  TEST_ASSERT_EQUAL_INT64(1500000, stats->sampling_frequency[1]);

  float complex input[] = {0.25F + 0.0F * I};
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_tx(input, 1, device->plugin));
  int16_t *actual = NULL;
  size_t actual_len = 0;
  iio_lib_mock_get_tx(&actual, &actual_len);
  const int16_t expected[] = {8192, 0};
  assert_int16_array(expected, 2, actual, actual_len);
}

void test_tx_larger_than_buffer() {
  // buffer is BUFFER_LEN / 2 samples per push, thus 10 samples need 3 pushes
  int16_t tx[40] = {0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, tx));
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  float complex input[10];
  for (int i = 0; i < 10; i++) {
    input[i] = (float) i / 16.0F + 0.0F * I;
  }
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_tx(input, 10, device->plugin));
  int16_t *actual = NULL;
  size_t actual_len = 0;
  iio_lib_mock_get_tx(&actual, &actual_len);
  TEST_ASSERT_EQUAL_size_t(20, actual_len);
  for (int i = 0; i < 10; i++) {
    TEST_ASSERT_EQUAL_INT16(i * 2048, actual[2 * i]);
    TEST_ASSERT_EQUAL_INT16(0, actual[2 * i + 1]);
  }
}

void test_tx_not_initialized() {
  int16_t tx[10] = {0};
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, tx));
  float complex input[] = {0.25F};
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_tx(input, 1, device->plugin));
}

void test_tx_invalid_frequency() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(69999999, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
  cfg.frequency = 6000000001ULL;
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->frequency_writes);
}

void test_tx_invalid_sample_rate() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 520833, -10);
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
  cfg.sample_rate = 61440001;
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
}

void test_tx_invalid_power() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  // tx power range is [-89.75;0], rx gain range [-1;73] must not be used
  sdr_channel_config cfg = config(433000000, 3000000, 1);
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
  cfg.gain = -90;
  TEST_ASSERT_EQUAL_INT(-1, device->set_tx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->hardwaregain_writes[1]);
  cfg.gain = -89.75F;
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  cfg.gain = 0;
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
}

void test_tx_power_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  iio_lib_mock_fail_write("hardwaregain");
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_TRUE(device->set_tx_parameters(&cfg, device->plugin) < 0);
}

void test_tx_applies_with_any_rx_gain_mode() {
  // rx gain mode must not affect tx power
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_HYBRID, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, iio_lib_mock_get_stats()->hardwaregain_writes[1]);
}

void test_tx_powerdown() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  plutosdr_settings settings = {.tx_powerdown = true, .gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local"};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_EQUAL_INT(1, iio_lib_mock_get_stats()->powerdown_writes);
}

void test_tx_enables_lo_after_powerdown() {
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_create(NULL, 0, NULL, &lib));
  plutosdr_settings settings = {.tx_powerdown = true, .gain_control_mode = IIO_GAIN_MODE_MANUAL, .timeout_ms = 1000, .uri = (char *) "ip:pluto.local"};
  TEST_ASSERT_EQUAL_INT(0, plutosdr_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_EQUAL_INT64(1, iio_lib_mock_get_stats()->powerdown);
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(2, iio_lib_mock_get_stats()->powerdown_writes);
  TEST_ASSERT_EQUAL_INT64(0, iio_lib_mock_get_stats()->powerdown);
}

void test_tx_lo_enable_failure() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  iio_lib_mock_fail_write("powerdown");
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_TRUE(device->set_tx_parameters(&cfg, device->plugin) < 0);
  TEST_ASSERT_EQUAL_INT(0, iio_lib_mock_get_stats()->frequency_writes);
}

void test_destroy_stops_tx_carrier() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  sdr_channel_config cfg = config(433000000, 3000000, -10);
  TEST_ASSERT_EQUAL_INT(0, device->set_tx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT64(0, iio_lib_mock_get_stats()->powerdown);
  TEST_ASSERT_EQUAL_FLOAT(-10, iio_lib_mock_get_stats()->hardwaregain[1]);

  device->destroy(device->plugin);
  free(device);
  device = NULL;
  // the device keeps transmitting after the context is closed unless tx is explicitly shut down
  TEST_ASSERT_EQUAL_INT64(1, iio_lib_mock_get_stats()->powerdown);
  TEST_ASSERT_EQUAL_FLOAT(-89.75, iio_lib_mock_get_stats()->hardwaregain[1]);
}

void test_destroy_without_tx_session() {
  TEST_ASSERT_EQUAL_INT(0, create_device(0, IIO_GAIN_MODE_MANUAL, NULL, 0, NULL));
  device->destroy(device->plugin);
  free(device);
  device = NULL;
  TEST_ASSERT_EQUAL_INT64(1, iio_lib_mock_get_stats()->powerdown);
}

void tearDown() {
  if (device != NULL) {
    device->destroy(device->plugin);
    free(device);
    device = NULL;
  }
  if (lib != NULL) {
    iio_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void setUp() {
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_default_uri);
  RUN_TEST(test_create_default_calib_mode);
  RUN_TEST(test_create_calib_mode);
  RUN_TEST(test_create_empty_calib_mode);
  RUN_TEST(test_create_calib_mode_before_sample_rate);
  RUN_TEST(test_create_calib_mode_write_failure);
  RUN_TEST(test_create_gain_modes);
  RUN_TEST(test_create_invalid_gain_mode);
  RUN_TEST(test_create_gain_mode_write_failure);
  RUN_TEST(test_create_invalid_range_format);
  RUN_TEST(test_create_default_sample_rate_without_fir);
  RUN_TEST(test_create_default_sample_rate_with_fir);
  RUN_TEST(test_create_default_sample_rate_too_low);
  RUN_TEST(test_create_default_sample_rate_same_as_current);
  RUN_TEST(test_rx_sample_rate_without_fir);
  RUN_TEST(test_rx_sample_rate_boundaries);
  RUN_TEST(test_rx_sample_rate_down_to_fir_and_back_up);
  RUN_TEST(test_rx_same_sample_rate_not_rewritten);
  RUN_TEST(test_rx_invalid_frequency);
  RUN_TEST(test_rx_invalid_sample_rate);
  RUN_TEST(test_rx_invalid_gain);
  RUN_TEST(test_rx_gain_write_failure);
  RUN_TEST(test_rx_frequency_write_failure);
  RUN_TEST(test_rx_automatic_gain_mode);
  RUN_TEST(test_rx_success);
  RUN_TEST(test_rx_success_with_fir);
  RUN_TEST(test_rx_not_started);
  RUN_TEST(test_rx_buffer_overflow);
  RUN_TEST(test_rx_restart);
  RUN_TEST(test_tx_success);
  RUN_TEST(test_tx_success_with_fir);
  RUN_TEST(test_tx_larger_than_buffer);
  RUN_TEST(test_tx_not_initialized);
  RUN_TEST(test_tx_invalid_frequency);
  RUN_TEST(test_tx_invalid_sample_rate);
  RUN_TEST(test_tx_invalid_power);
  RUN_TEST(test_tx_power_write_failure);
  RUN_TEST(test_tx_applies_with_any_rx_gain_mode);
  RUN_TEST(test_tx_powerdown);
  RUN_TEST(test_tx_enables_lo_after_powerdown);
  RUN_TEST(test_tx_lo_enable_failure);
  RUN_TEST(test_destroy_stops_tx_carrier);
  RUN_TEST(test_destroy_without_tx_session);
  return UNITY_END();
}
