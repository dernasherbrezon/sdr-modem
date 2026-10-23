#include <stdlib.h>
#include <unity.h>
#include <math.h>
#include <errno.h>
#include "../src/app_config.h"
#include "../src/sdr/airspy_lib.h"

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

void test_unknown_sdr_type() {
  char *argv[] = {"test_app_config", "--config", "invalid.sdr_type.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_pluto_enabled() {
  char *argv[] = {"test_app_config", "--config", "pluto_enabled.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_PLUTOSDR, config->sdr_type);
  TEST_ASSERT(config->iio != NULL);
  TEST_ASSERT(fabsl(10.0 - config->gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(20000, config->plutosdr.timeout_ms);
}

// airspy

// libairspy is optional. tests which load the library can only run where it is installed
static void ignore_if_no_libairspy() {
  airspy_lib *lib = NULL;
  if (airspy_lib_create(&lib) != 0) {
    TEST_IGNORE_MESSAGE("libairspy is not installed");
  }
  airspy_lib_destroy(lib);
}

void test_airspy_enabled() {
  ignore_if_no_libairspy();
  char *argv[] = {"test_app_config", "--config", "airspy_enabled.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_AIRSPY, config->sdr_type);
  TEST_ASSERT(config->airspy_lib != NULL);
  TEST_ASSERT(config->iio == NULL);
  TEST_ASSERT_EQUAL_INT(AIRSPY_GAIN_MODE_MANUAL, config->airspy.gain_mode);
  TEST_ASSERT_EQUAL_INT(11, config->airspy.lna_gain);
  TEST_ASSERT_EQUAL_INT(12, config->airspy.mixer_gain);
  TEST_ASSERT_EQUAL_INT(13, config->airspy.vga_gain);
  TEST_ASSERT_TRUE(config->airspy.bias_tee);
  TEST_ASSERT_EQUAL_UINT64(0x1234ABCD5678EF90ULL, config->airspy.serial_number);
}

void test_airspy_defaults() {
  ignore_if_no_libairspy();
  char *argv[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", "--max_frame_size", "2048", NULL};
  int code = app_config_create(7, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(AIRSPY_GAIN_MODE_AUTO, config->airspy.gain_mode);
  TEST_ASSERT_EQUAL_UINT64(0, config->airspy.serial_number);
  TEST_ASSERT_FALSE(config->airspy.bias_tee);
}

void test_airspy_gain_modes_from_cli() {
  ignore_if_no_libairspy();
  struct {
    const char *name;
    airspy_gain_mode mode;
  } cases[] = {
      {"auto", AIRSPY_GAIN_MODE_AUTO},
      {"sensitivity", AIRSPY_GAIN_MODE_SENSITIVITY},
      {"linearity", AIRSPY_GAIN_MODE_LINEARITY},
      {"manual", AIRSPY_GAIN_MODE_MANUAL},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char *argv[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", "--max_frame_size", "2048", "--airspy_gain_mode", (char *) cases[i].name, NULL};
    TEST_ASSERT_EQUAL_INT(0, app_config_create(9, argv, &config));
    TEST_ASSERT_EQUAL_INT(cases[i].mode, config->airspy.gain_mode);
    app_config_destroy(config);
    config = NULL;
  }
}

void test_airspy_override_from_cli() {
  ignore_if_no_libairspy();
  char *argv[] = {"test_app_config", "--config", "airspy_enabled.conf", "--airspy_gain_mode", "linearity", "--airspy_lna_gain", "3",
                  "--airspy_mixer_gain", "0", "--airspy_vga_gain", "15", "--airspy_bias_tee", "false",
                  "--airspy_serial", "12345", NULL};
  TEST_ASSERT_EQUAL_INT(0, app_config_create(15, argv, &config));
  TEST_ASSERT_EQUAL_INT(AIRSPY_GAIN_MODE_LINEARITY, config->airspy.gain_mode);
  TEST_ASSERT_EQUAL_INT(3, config->airspy.lna_gain);
  TEST_ASSERT_EQUAL_INT(0, config->airspy.mixer_gain);
  TEST_ASSERT_EQUAL_INT(15, config->airspy.vga_gain);
  TEST_ASSERT_FALSE(config->airspy.bias_tee);
  TEST_ASSERT_EQUAL_UINT64(12345, config->airspy.serial_number);
}

void test_airspy_cli_mode_rx() {
  ignore_if_no_libairspy();
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "airspy", "--output", "/some-path", "--gain", "7", NULL};
  TEST_ASSERT_EQUAL_INT(0, app_config_create(11, argv, &config));
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_AIRSPY, config->sdr_type);
  // the sample rate of the modem is requested from the device
  TEST_ASSERT_EQUAL_UINT64(48000, config->airspy.sample_rate);
  TEST_ASSERT(fabsl(7.0 - config->gain) < 0.001);
}

void test_airspy_tx_not_supported() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "tx", "--sdr_type", "airspy", "--input", "/some-path", NULL};
  TEST_ASSERT_EQUAL_INT(-1, app_config_create(9, argv, &config));
}

void test_airspy_invalid_gain_mode() {
  char *argv1[] = {"test_app_config", "--config", "airspy_invalid_gain_mode.conf", NULL};
  TEST_ASSERT_EQUAL_INT(-1, app_config_create(3, argv1, &config));
  char *argv2[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", "--airspy_gain_mode", "turbo", NULL};
  TEST_ASSERT_EQUAL_INT(-1, app_config_create(7, argv2, &config));
  // plutosdr modes are not accepted
  char *argv3[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", "--airspy_gain_mode", "hybrid", NULL};
  TEST_ASSERT_EQUAL_INT(-1, app_config_create(7, argv3, &config));
}

void test_airspy_invalid_gains() {
  char *argv1[] = {"test_app_config", "--config", "airspy_invalid_lna_gain.conf", NULL};
  TEST_ASSERT_EQUAL_INT(-1, app_config_create(3, argv1, &config));
  const char *options[] = {"--airspy_lna_gain", "--airspy_mixer_gain", "--airspy_vga_gain"};
  const char *values[][3] = {{"15", "16", "16"}, {"-1", "-1", "-1"}, {"abc", "abc", "abc"}, {"", "", ""}, {"1x", "1x", "1x"}};
  for (size_t o = 0; o < 3; o++) {
    for (size_t v = 0; v < 5; v++) {
      char *argv[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", (char *) options[o], (char *) values[v][o], NULL};
      TEST_ASSERT_EQUAL_INT_MESSAGE(-1, app_config_create(7, argv, &config), values[v][o]);
    }
  }
}

void test_airspy_invalid_serial() {
  const char *values[] = {"abc", "", "-1", "12x", "99999999999999999999999"};
  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    char *argv[] = {"test_app_config", "--bind_address", "127.0.0.1", "--sdr_type", "airspy", "--airspy_serial", (char *) values[i], NULL};
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, app_config_create(7, argv, &config), values[i]);
  }
}

void test_success() {
  char *argv[] = {"test_app_config", "--config", "full.conf", NULL};
  int code = app_config_create(3, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", config->bind_address);
  TEST_ASSERT_EQUAL_INT(8091, config->port);
  TEST_ASSERT_EQUAL_INT(10, config->read_timeout_seconds);
  TEST_ASSERT_EQUAL_INT(2048, config->sdr_buffer_len);
  TEST_ASSERT_EQUAL_INT(SDR_TYPE_PLUTOSDR, config->sdr_type);
  TEST_ASSERT_EQUAL_INT(64, config->queue_size);
  TEST_ASSERT(config->iio != NULL);
  TEST_ASSERT(fabsl(0.0 - config->gain) < 0.001);
  TEST_ASSERT_EQUAL_INT(10000, config->plutosdr.timeout_ms);
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
  char *argv1[] = {"test_app_config", "--sdr_type", "file", "--bind_address", "127.0.0.1", NULL};
  int code = app_config_create(5, argv1, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);

  char *argv2[] = {"test_app_config", "--sdr_type", "file", NULL};
  code = app_config_create(3, argv2, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_merge_gfsk_settings() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "/non-existing-directory/in.cf32", "--output", "/some-path", "--gfsk_baud_rate", "4800", NULL};
  int code = app_config_create(13, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(4800, config->gfsk.baud_rate);
}

void test_frequency() {
  char *argv1[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv1, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(437525000, config->frequency);
  app_config_destroy(config);

  char *argv2[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--frequency", "145800000", NULL};
  code = app_config_create(13, argv2, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(145800000, config->frequency);
}

void test_sample_rate() {
  char *argv1[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv1, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(48000, config->sample_rate);
  app_config_destroy(config);

  char *argv2[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--sample_rate", "96000", NULL};
  code = app_config_create(13, argv2, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(96000, config->sample_rate);
}

void test_guess_rx_file_format_cf32() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(FILE_FORMAT_CF32, config->sdr_file.rx_file_format);
}

void test_guess_rx_file_format_cf32_gz() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32.gz", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(FILE_FORMAT_CF32, config->sdr_file.rx_file_format);
}

void test_guess_rx_file_format_cu8() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cu8", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(FILE_FORMAT_CU8, config->sdr_file.rx_file_format);
}

void test_guess_rx_file_format_cs16() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cs16", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(FILE_FORMAT_CS16, config->sdr_file.rx_file_format);
}

void test_guess_rx_file_format_unknown() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.raw", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_guess_tx_file_format_cu8() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "tx", "--sdr_type", "file", "--tx_file", "out.cu8", "--input", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(FILE_FORMAT_CU8, config->sdr_file.tx_file_format);
}

void test_syncword_from_file() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", NULL};
  int code = app_config_create(11, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(0xCCCCCCFE, config->gfsk.syncword);
  TEST_ASSERT_EQUAL_UINT32(32, config->gfsk.syncword_bits);
}

void test_merge_syncword() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--syncword", "0xaa7", NULL};
  int code = app_config_create(13, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(0xAA7, config->gfsk.syncword);
  TEST_ASSERT_EQUAL_UINT32(12, config->gfsk.syncword_bits);
}

void test_max_syncword() {
  char *argv[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--syncword", "FEDCBA9876543210", NULL};
  int code = app_config_create(13, argv, &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_UINT64(0xFEDCBA9876543210ULL, config->gfsk.syncword);
  TEST_ASSERT_EQUAL_UINT32(64, config->gfsk.syncword_bits);
}

void test_invalid_syncword() {
  char *argv1[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--syncword", "AAX7", NULL};
  TEST_ASSERT_EQUAL_INT(-EINVAL, app_config_create(13, argv1, &config));
  char *argv2[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--syncword", "0x", NULL};
  TEST_ASSERT_EQUAL_INT(-EINVAL, app_config_create(13, argv2, &config));
  char *argv3[] = {"test_app_config", "--config", "cli.conf", "--direction", "rx", "--sdr_type", "file", "--rx_file", "in.cf32", "--output", "/some-path", "--syncword", "00112233445566778", NULL};
  TEST_ASSERT_EQUAL_INT(-EINVAL, app_config_create(13, argv3, &config));
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
  RUN_TEST(test_invalid_timeout);
  RUN_TEST(test_invalid_format);
  RUN_TEST(test_missing_file);
  RUN_TEST(test_unknown_sdr_type);
  RUN_TEST(test_pluto_enabled);
  RUN_TEST(test_airspy_enabled);
  RUN_TEST(test_airspy_defaults);
  RUN_TEST(test_airspy_gain_modes_from_cli);
  RUN_TEST(test_airspy_override_from_cli);
  RUN_TEST(test_airspy_cli_mode_rx);
  RUN_TEST(test_airspy_tx_not_supported);
  RUN_TEST(test_airspy_invalid_gain_mode);
  RUN_TEST(test_airspy_invalid_gains);
  RUN_TEST(test_airspy_invalid_serial);
  RUN_TEST(test_override_from_cli);
  RUN_TEST(test_override_with_invalid);
  RUN_TEST(test_invalid);
  RUN_TEST(test_merge_gfsk_settings);
  RUN_TEST(test_frequency);
  RUN_TEST(test_sample_rate);
  RUN_TEST(test_guess_rx_file_format_cf32);
  RUN_TEST(test_guess_rx_file_format_cf32_gz);
  RUN_TEST(test_guess_rx_file_format_cu8);
  RUN_TEST(test_guess_rx_file_format_cs16);
  RUN_TEST(test_guess_rx_file_format_unknown);
  RUN_TEST(test_guess_tx_file_format_cu8);
  RUN_TEST(test_syncword_from_file);
  RUN_TEST(test_merge_syncword);
  RUN_TEST(test_max_syncword);
  RUN_TEST(test_invalid_syncword);
  return UNITY_END();
}
