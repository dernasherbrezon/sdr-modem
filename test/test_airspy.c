#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <unity.h>
#include "../src/sdr/airspy.h"
#include "airspy_lib_mock.h"

#define BUFFER_LEN 8
// ring capacity in the driver is at least this much
#define RING_LEN 262144

sdr_device *device = NULL;
airspy_lib *lib = NULL;

static airspy_settings default_settings(void) {
  return (airspy_settings) {
      .gain_mode = AIRSPY_GAIN_MODE_AUTO,
      .timeout_ms = 100,
  };
}

static int create_with(airspy_settings settings) {
  if (lib == NULL) {
    TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  }
  return airspy_create(&settings, BUFFER_LEN, lib, &device);
}

static void create_ok(airspy_settings settings) {
  TEST_ASSERT_EQUAL_INT(0, create_with(settings));
  TEST_ASSERT_NOT_NULL(device);
}

static sdr_channel_config config(uint64_t frequency, uint64_t sample_rate, float gain) {
  return (sdr_channel_config) {.frequency = frequency, .sample_rate = sample_rate, .gain = gain};
}

static void deliver_sequence(int start, int count) {
  float complex *samples = malloc(sizeof(float complex) * count);
  TEST_ASSERT_NOT_NULL(samples);
  for (int i = 0; i < count; i++) {
    samples[i] = (float) (start + i) + I * (float) (-(start + i));
  }
  airspy_lib_mock_deliver(samples, count, 0, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  free(samples);
}

static void assert_buffer(const float complex *output, int start) {
  for (int i = 0; i < BUFFER_LEN; i++) {
    TEST_ASSERT_EQUAL_FLOAT((float) (start + i), crealf(output[i]));
    TEST_ASSERT_EQUAL_FLOAT((float) (-(start + i)), cimagf(output[i]));
  }
}

// create /////////////////////////////////////////////////////////

void test_create_defaults() {
  create_ok(default_settings());
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(1, stats->open_calls);
  TEST_ASSERT_EQUAL_INT(0, stats->open_sn_calls);
  TEST_ASSERT_EQUAL_INT(1, stats->sample_type_calls);
  TEST_ASSERT_EQUAL_INT(AIRSPY_LIB_SAMPLE_FLOAT32_IQ, stats->sample_type);
  TEST_ASSERT_EQUAL_INT(1, stats->rf_bias_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->rf_bias);
  // sample rate 0 means "leave as is"
  TEST_ASSERT_EQUAL_INT(0, stats->sample_rate_writes);
}

void test_create_with_serial_number() {
  airspy_settings settings = default_settings();
  settings.serial_number = 0x1234ABCD5678EF90ULL;
  create_ok(settings);
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->open_calls);
  TEST_ASSERT_EQUAL_INT(1, stats->open_sn_calls);
  TEST_ASSERT_EQUAL_UINT64(0x1234ABCD5678EF90ULL, stats->serial_number);
}

void test_create_open_failure() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  airspy_lib_mock_fail("airspy_open");
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&(airspy_settings) {.timeout_ms = 100}, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
  // never opened, thus nothing to close
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->close_calls);
}

void test_create_open_serial_failure() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  airspy_lib_mock_fail("airspy_open_sn");
  airspy_settings settings = default_settings();
  settings.serial_number = 1;
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->close_calls);
}

void test_create_closes_device_on_failure() {
  const char *failures[] = {"airspy_set_sample_type", "airspy_get_samplerates", "airspy_set_rf_bias"};
  for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
    TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
    airspy_lib_mock_fail(failures[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, airspy_create(&(airspy_settings) {.timeout_ms = 100}, BUFFER_LEN, lib, &device), failures[i]);
    TEST_ASSERT_NULL(device);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, airspy_lib_mock_get_stats()->close_calls, failures[i]);
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_create_get_samplerates_second_call_failure() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  // first call returns the number of rates, second one the rates
  airspy_lib_mock_fail_on_call("airspy_get_samplerates", 2);
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&(airspy_settings) {.timeout_ms = 100}, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->close_calls);
}

void test_create_no_sample_rates() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  airspy_lib_mock_set_sample_rates(NULL, 0);
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&(airspy_settings) {.timeout_ms = 100}, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->close_calls);
}

void test_create_bias_tee() {
  airspy_settings settings = default_settings();
  settings.bias_tee = true;
  create_ok(settings);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->rf_bias);
}

void test_create_invalid_settings_do_not_open_device() {
  airspy_settings cases[4];
  for (int i = 0; i < 4; i++) {
    cases[i] = default_settings();
  }
  cases[0].gain_mode = (airspy_gain_mode) 100;
  cases[1].lna_gain = AIRSPY_MAX_LNA_GAIN + 1;
  cases[2].mixer_gain = AIRSPY_MAX_MIXER_GAIN + 1;
  cases[3].vga_gain = AIRSPY_MAX_VGA_GAIN + 1;
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, airspy_create(&cases[i], BUFFER_LEN, lib, &device), "invalid settings were accepted");
    TEST_ASSERT_NULL(device);
    TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->open_calls);
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_create_max_gains_are_valid() {
  airspy_settings settings = default_settings();
  settings.gain_mode = AIRSPY_GAIN_MODE_MANUAL;
  settings.lna_gain = AIRSPY_MAX_LNA_GAIN;
  settings.mixer_gain = AIRSPY_MAX_MIXER_GAIN;
  settings.vga_gain = AIRSPY_MAX_VGA_GAIN;
  create_ok(settings);
}

void test_create_zero_buffer_len() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  airspy_settings settings = default_settings();
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&settings, 0, lib, &device));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->open_calls);
}

void test_create_default_sample_rate() {
  airspy_settings settings = default_settings();
  settings.sample_rate = 10000000;
  create_ok(settings);
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(1, stats->sample_rate_writes);
  TEST_ASSERT_EQUAL_UINT32(10000000, stats->sample_rate);
}

void test_create_default_sample_rate_not_supported() {
  airspy_settings settings = default_settings();
  settings.sample_rate = 48000;
  TEST_ASSERT_EQUAL_INT(-1, create_with(settings));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->sample_rate_writes);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->close_calls);
}

void test_create_default_sample_rate_write_failure() {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  airspy_lib_mock_fail("airspy_set_samplerate");
  airspy_settings settings = default_settings();
  settings.sample_rate = 2500000;
  TEST_ASSERT_EQUAL_INT(-1, airspy_create(&settings, BUFFER_LEN, lib, &device));
  TEST_ASSERT_NULL(device);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->close_calls);
}

void test_destroy_null() {
  // must not crash
  airspy_settings settings = default_settings();
  create_ok(settings);
  device->destroy(NULL);
}

// rx parameters //////////////////////////////////////////////////

void test_rx_auto_gain() {
  airspy_settings settings = default_settings();
  settings.vga_gain = 7;
  create_ok(settings);
  sdr_channel_config cfg = config(433000000, 2500000, 100);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_UINT32(433000000, stats->freq);
  TEST_ASSERT_EQUAL_UINT32(2500000, stats->sample_rate);
  TEST_ASSERT_EQUAL_INT(1, stats->lna_agc);
  TEST_ASSERT_EQUAL_INT(1, stats->mixer_agc);
  TEST_ASSERT_EQUAL_INT(7, stats->vga_gain);
  // requested gain doesn't matter in the auto mode, even out of range
  TEST_ASSERT_EQUAL_INT(0, stats->sensitivity_gain_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->linearity_gain_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->lna_gain_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->mixer_gain_writes);
}

void test_rx_sensitivity_gain() {
  airspy_settings settings = default_settings();
  settings.gain_mode = AIRSPY_GAIN_MODE_SENSITIVITY;
  create_ok(settings);
  sdr_channel_config cfg = config(433000000, 2500000, 12.4F);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->lna_agc);
  TEST_ASSERT_EQUAL_INT(0, stats->mixer_agc);
  TEST_ASSERT_EQUAL_INT(1, stats->sensitivity_gain_writes);
  TEST_ASSERT_EQUAL_INT(12, stats->sensitivity_gain);
  TEST_ASSERT_EQUAL_INT(0, stats->linearity_gain_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->vga_gain_writes);

  cfg.gain = 12.6F;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(13, stats->sensitivity_gain);
}

void test_rx_linearity_gain() {
  airspy_settings settings = default_settings();
  settings.gain_mode = AIRSPY_GAIN_MODE_LINEARITY;
  create_ok(settings);
  sdr_channel_config cfg = config(433000000, 2500000, 5);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->lna_agc);
  TEST_ASSERT_EQUAL_INT(0, stats->mixer_agc);
  TEST_ASSERT_EQUAL_INT(1, stats->linearity_gain_writes);
  TEST_ASSERT_EQUAL_INT(5, stats->linearity_gain);
  TEST_ASSERT_EQUAL_INT(0, stats->sensitivity_gain_writes);
}

void test_rx_manual_gain() {
  airspy_settings settings = default_settings();
  settings.gain_mode = AIRSPY_GAIN_MODE_MANUAL;
  settings.lna_gain = 11;
  settings.mixer_gain = 12;
  settings.vga_gain = 13;
  create_ok(settings);
  // requested gain is ignored in the manual mode, even if it is out of range
  sdr_channel_config cfg = config(433000000, 2500000, 1000);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->lna_agc);
  TEST_ASSERT_EQUAL_INT(0, stats->mixer_agc);
  TEST_ASSERT_EQUAL_INT(11, stats->lna_gain);
  TEST_ASSERT_EQUAL_INT(12, stats->mixer_gain);
  TEST_ASSERT_EQUAL_INT(13, stats->vga_gain);
  TEST_ASSERT_EQUAL_INT(0, stats->sensitivity_gain_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->linearity_gain_writes);
}

void test_rx_combined_gain_boundaries() {
  airspy_gain_mode modes[] = {AIRSPY_GAIN_MODE_SENSITIVITY, AIRSPY_GAIN_MODE_LINEARITY};
  for (size_t m = 0; m < 2; m++) {
    airspy_settings settings = default_settings();
    settings.gain_mode = modes[m];
    create_ok(settings);
    struct {
      float gain;
      bool valid;
    } cases[] = {
        {0, true},
        {21, true},
        {-0.01F, false},
        {21.01F, false},
        {-1, false},
        {22, false},
        {NAN, false},
        {INFINITY, false},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      sdr_channel_config cfg = config(433000000, 2500000, cases[i].gain);
      int code = device->set_rx_parameters(&cfg, device->plugin);
      if (cases[i].valid) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, code, "valid gain was rejected");
      } else {
        TEST_ASSERT_EQUAL_INT_MESSAGE(-1, code, "invalid gain was accepted");
      }
    }
    device->destroy(device->plugin);
    free(device);
    device = NULL;
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_rx_frequency_boundaries() {
  create_ok(default_settings());
  struct {
    uint64_t frequency;
    bool valid;
  } cases[] = {
      {24000000, true},
      {1750000000, true},
      {23999999, false},
      {1750000001, false},
      {0, false},
      {4294967296ULL + 433000000ULL, false}, // would wrap around if cast to uint32 before the check
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    sdr_channel_config cfg = config(cases[i].frequency, 2500000, 0);
    int code = device->set_rx_parameters(&cfg, device->plugin);
    TEST_ASSERT_EQUAL_INT_MESSAGE(cases[i].valid ? 0 : -1, code, "unexpected frequency validation result");
  }
}

void test_rx_invalid_parameters_do_not_touch_device() {
  airspy_settings settings = default_settings();
  settings.gain_mode = AIRSPY_GAIN_MODE_SENSITIVITY;
  create_ok(settings);
  sdr_channel_config bad_frequency = config(1, 2500000, 5);
  sdr_channel_config bad_sample_rate = config(433000000, 48000, 5);
  sdr_channel_config bad_gain = config(433000000, 2500000, 22);
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&bad_frequency, device->plugin));
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&bad_sample_rate, device->plugin));
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&bad_gain, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->freq_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->sample_rate_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->lna_agc_writes);
  TEST_ASSERT_EQUAL_INT(0, stats->sensitivity_gain_writes);
}

void test_rx_sample_rates() {
  create_ok(default_settings());
  sdr_channel_config cfg = config(433000000, 2500000, 0);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(1, stats->sample_rate_writes);
  // same sample rate is not rewritten
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, stats->sample_rate_writes);
  // changed
  cfg.sample_rate = 10000000;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(2, stats->sample_rate_writes);
  TEST_ASSERT_EQUAL_UINT32(10000000, stats->sample_rate);
  // and back
  cfg.sample_rate = 2500000;
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(3, stats->sample_rate_writes);
  TEST_ASSERT_EQUAL_UINT32(2500000, stats->sample_rate);
}

void test_rx_default_sample_rate_not_rewritten() {
  airspy_settings settings = default_settings();
  settings.sample_rate = 10000000;
  create_ok(settings);
  sdr_channel_config cfg = config(433000000, 10000000, 0);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->sample_rate_writes);
}

void test_rx_sample_rate_must_match_exactly() {
  create_ok(default_settings());
  sdr_channel_config cfg = config(433000000, 2500001, 0);
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.sample_rate = 2499999;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  cfg.sample_rate = 0;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
}

void test_rx_device_specific_sample_rates() {
  // airspy mini
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  uint32_t rates[] = {6000000, 3000000};
  airspy_lib_mock_set_sample_rates(rates, 2);
  create_ok(default_settings());
  sdr_channel_config cfg = config(433000000, 3000000, 0);
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  cfg.sample_rate = 2500000;
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
}

void test_rx_write_failures() {
  struct {
    const char *function;
    airspy_gain_mode mode;
  } cases[] = {
      {"airspy_set_samplerate", AIRSPY_GAIN_MODE_AUTO},
      {"airspy_set_freq", AIRSPY_GAIN_MODE_AUTO},
      {"airspy_set_lna_agc", AIRSPY_GAIN_MODE_AUTO},
      {"airspy_set_mixer_agc", AIRSPY_GAIN_MODE_AUTO},
      {"airspy_set_vga_gain", AIRSPY_GAIN_MODE_AUTO},
      {"airspy_set_sensitivity_gain", AIRSPY_GAIN_MODE_SENSITIVITY},
      {"airspy_set_lna_agc", AIRSPY_GAIN_MODE_SENSITIVITY},
      {"airspy_set_linearity_gain", AIRSPY_GAIN_MODE_LINEARITY},
      {"airspy_set_mixer_agc", AIRSPY_GAIN_MODE_LINEARITY},
      {"airspy_set_lna_gain", AIRSPY_GAIN_MODE_MANUAL},
      {"airspy_set_mixer_gain", AIRSPY_GAIN_MODE_MANUAL},
      {"airspy_set_vga_gain", AIRSPY_GAIN_MODE_MANUAL},
      {"airspy_set_lna_agc", AIRSPY_GAIN_MODE_MANUAL},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    airspy_settings settings = default_settings();
    settings.gain_mode = cases[i].mode;
    create_ok(settings);
    airspy_lib_mock_fail(cases[i].function);
    sdr_channel_config cfg = config(433000000, 2500000, 5);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-1, device->set_rx_parameters(&cfg, device->plugin), cases[i].function);
    device->destroy(device->plugin);
    free(device);
    device = NULL;
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void test_rx_sample_rate_retried_after_failure() {
  create_ok(default_settings());
  sdr_channel_config cfg = config(433000000, 2500000, 0);
  airspy_lib_mock_fail("airspy_set_samplerate");
  TEST_ASSERT_EQUAL_INT(-1, device->set_rx_parameters(&cfg, device->plugin));
  airspy_lib_mock_fail(NULL);
  // the failed write must not be remembered as the current sample rate
  TEST_ASSERT_EQUAL_INT(0, device->set_rx_parameters(&cfg, device->plugin));
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->sample_rate_writes);
}

// rx /////////////////////////////////////////////////////////////

static void start_rx(void) {
  create_ok(default_settings());
  device->start_rx(device->plugin);
}

void test_rx_not_started() {
  create_ok(default_settings());
  float complex *output = (float complex *) 0x1;
  size_t output_len = 100;
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
}

void test_rx_success() {
  start_rx();
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->start_rx_calls);
  // 2.5 buffers
  deliver_sequence(0, BUFFER_LEN * 2 + BUFFER_LEN / 2);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_EQUAL_size_t(BUFFER_LEN, output_len);
  assert_buffer(output, 0);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_EQUAL_size_t(BUFFER_LEN, output_len);
  assert_buffer(output, BUFFER_LEN);
  // the remaining half buffer is not enough
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
  // and it is not lost
  deliver_sequence(BUFFER_LEN * 2 + BUFFER_LEN / 2, BUFFER_LEN / 2);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  assert_buffer(output, BUFFER_LEN * 2);
}

void test_rx_blocks_are_not_aligned_with_buffer() {
  start_rx();
  // blocks of 3 samples. buffer is 8
  for (int i = 0; i < 8; i++) {
    deliver_sequence(i * 3, 3);
  }
  float complex *output = NULL;
  size_t output_len = 0;
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
    assert_buffer(output, i * BUFFER_LEN);
  }
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_ring_wrap_around() {
  start_rx();
  float complex *output = NULL;
  size_t output_len = 0;
  int total = 200000;
  deliver_sequence(0, total);
  for (int i = 0; i < total; i += BUFFER_LEN) {
    TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
    assert_buffer(output, i);
  }
  // write position is at 200000 and the block crosses the end of the ring
  int more = 100000;
  TEST_ASSERT_TRUE(total + more > RING_LEN);
  deliver_sequence(total, more);
  for (int i = total; i < total + more; i += BUFFER_LEN) {
    TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
    assert_buffer(output, i);
  }
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_overflow_drops_new_blocks() {
  start_rx();
  // fill the ring completely
  deliver_sequence(0, RING_LEN);
  // not a single sample fits
  deliver_sequence(RING_LEN, BUFFER_LEN);
  float complex *output = NULL;
  size_t output_len = 0;
  // the oldest data is intact
  for (int i = 0; i < RING_LEN; i += BUFFER_LEN) {
    TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
    assert_buffer(output, i);
  }
  // and the dropped block did not leak in
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  // ring is usable again
  deliver_sequence(1000, BUFFER_LEN);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  assert_buffer(output, 1000);
}

void test_rx_block_larger_than_ring_is_dropped() {
  start_rx();
  deliver_sequence(0, RING_LEN + 1);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_partial_block_is_never_written() {
  start_rx();
  deliver_sequence(0, RING_LEN - 4);
  // only 4 samples left, the block of 8 is dropped entirely
  deliver_sequence(RING_LEN - 4, BUFFER_LEN);
  deliver_sequence(RING_LEN - 4, 4);
  float complex *output = NULL;
  size_t output_len = 0;
  // consume everything and check the stream is continuous
  int consumed = 0;
  while (consumed + BUFFER_LEN <= RING_LEN) {
    TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
    assert_buffer(output, consumed);
    consumed += BUFFER_LEN;
  }
}

void test_rx_device_reported_dropped_samples() {
  start_rx();
  float complex samples[BUFFER_LEN] = {0};
  // dropped samples are only reported (logged), the data still flows
  airspy_lib_mock_deliver(samples, BUFFER_LEN, 12345, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_EQUAL_size_t(BUFFER_LEN, output_len);
}

void test_rx_unexpected_sample_type_is_dropped() {
  start_rx();
  float complex samples[BUFFER_LEN] = {0};
  airspy_lib_mock_deliver(samples, BUFFER_LEN, 0, 2 /* int16 iq */);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_empty_and_negative_blocks() {
  start_rx();
  float complex samples[BUFFER_LEN] = {0};
  airspy_lib_mock_deliver(samples, 0, 0, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  airspy_lib_mock_deliver(samples, -5, 0, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  deliver_sequence(0, BUFFER_LEN);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  assert_buffer(output, 0);
}

void test_rx_timeout_and_device_stopped_streaming() {
  start_rx();
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  // e.g. usb cable was unplugged: libairspy stops streaming and no more callbacks happen
  airspy_lib_mock_set_streaming(false);
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
}

void test_rx_data_is_returned_even_if_device_stopped_streaming() {
  start_rx();
  deliver_sequence(0, BUFFER_LEN);
  airspy_lib_mock_set_streaming(false);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  assert_buffer(output, 0);
}

void test_rx_timeout_is_respected() {
  airspy_settings settings = default_settings();
  settings.timeout_ms = 1200;
  create_ok(settings);
  device->start_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  clock_gettime(CLOCK_MONOTONIC, &end);
  long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
  // sub-second part of the timeout and the carry into seconds are both applied
  TEST_ASSERT_TRUE(elapsed_ms >= 1100);
  TEST_ASSERT_TRUE(elapsed_ms < 3000);
}

void test_rx_default_timeout() {
  // timeout_ms=0 must not produce an instant timeout or an infinite wait
  airspy_settings settings = default_settings();
  settings.timeout_ms = 0;
  create_ok(settings);
  device->start_rx(device->plugin);
  deliver_sequence(0, BUFFER_LEN);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
}

static void *delayed_deliver(void *arg) {
  usleep(100 * 1000);
  deliver_sequence(0, BUFFER_LEN);
  return NULL;
}

void test_rx_waits_for_data() {
  airspy_settings settings = default_settings();
  settings.timeout_ms = 5000;
  create_ok(settings);
  device->start_rx(device->plugin);
  pthread_t thread;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_deliver, NULL));
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  pthread_join(thread, NULL);
  assert_buffer(output, 0);
}

static void *delayed_stop(void *arg) {
  usleep(100 * 1000);
  device->stop_rx(device->plugin);
  return NULL;
}

void test_rx_stop_wakes_up_waiting_reader() {
  airspy_settings settings = default_settings();
  settings.timeout_ms = 30000;
  create_ok(settings);
  device->start_rx(device->plugin);
  pthread_t thread;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_stop, NULL));
  float complex *output = NULL;
  size_t output_len = 0;
  // would take 30 seconds without the wake up
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  pthread_join(thread, NULL);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->stop_rx_calls);
}

void test_rx_stop() {
  start_rx();
  deliver_sequence(0, BUFFER_LEN);
  device->stop_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->stop_rx_calls);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  TEST_ASSERT_EQUAL_size_t(0, output_len);
}

void test_rx_stop_without_start() {
  create_ok(default_settings());
  device->stop_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->stop_rx_calls);
}

void test_rx_start_and_stop_are_idempotent() {
  start_rx();
  device->start_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->start_rx_calls);
  device->stop_rx(device->plugin);
  device->stop_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->stop_rx_calls);
}

void test_rx_restart_discards_old_data() {
  start_rx();
  deliver_sequence(0, BUFFER_LEN * 3);
  device->stop_rx(device->plugin);
  device->start_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(2, airspy_lib_mock_get_stats()->start_rx_calls);
  float complex *output = NULL;
  size_t output_len = 0;
  // stale data from the previous session must not be returned
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
  deliver_sequence(500, BUFFER_LEN);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
  assert_buffer(output, 500);
}

void test_rx_start_failure() {
  create_ok(default_settings());
  airspy_lib_mock_fail("airspy_start_rx");
  device->start_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
  // no streaming - nothing to stop
  device->stop_rx(device->plugin);
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->stop_rx_calls);
  // can be started again when the problem is gone
  airspy_lib_mock_fail(NULL);
  device->start_rx(device->plugin);
  deliver_sequence(0, BUFFER_LEN);
  TEST_ASSERT_EQUAL_INT(0, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_stop_failure() {
  start_rx();
  airspy_lib_mock_fail("airspy_stop_rx");
  device->stop_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  // data is not accepted anymore regardless of the library result
  TEST_ASSERT_EQUAL_INT(-1, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_rx_data_after_stop_is_ignored() {
  start_rx();
  float complex samples[BUFFER_LEN] = {0};
  // a block which was already in flight when stop was requested
  device->stop_rx(device->plugin);
  airspy_lib_mock_deliver(samples, BUFFER_LEN, 0, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  device->start_rx(device->plugin);
  float complex *output = NULL;
  size_t output_len = 0;
  TEST_ASSERT_EQUAL_INT(-ETIMEDOUT, device->sdr_process_rx(&output, &output_len, device->plugin));
}

void test_destroy_stops_rx_and_closes_device() {
  start_rx();
  device->destroy(device->plugin);
  free(device);
  device = NULL;
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(1, stats->stop_rx_calls);
  TEST_ASSERT_EQUAL_INT(1, stats->close_calls);
}

void test_destroy_not_started() {
  create_ok(default_settings());
  device->destroy(device->plugin);
  free(device);
  device = NULL;
  const airspy_lib_mock_stats *stats = airspy_lib_mock_get_stats();
  TEST_ASSERT_EQUAL_INT(0, stats->stop_rx_calls);
  TEST_ASSERT_EQUAL_INT(1, stats->close_calls);
}

// tx /////////////////////////////////////////////////////////////

void test_tx_not_supported() {
  create_ok(default_settings());
  TEST_ASSERT_NULL(device->set_tx_parameters);
  TEST_ASSERT_NULL(device->sdr_process_tx);
}

void tearDown() {
  if (device != NULL) {
    device->destroy(device->plugin);
    free(device);
    device = NULL;
  }
  if (lib != NULL) {
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

void setUp() {
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_defaults);
  RUN_TEST(test_create_with_serial_number);
  RUN_TEST(test_create_open_failure);
  RUN_TEST(test_create_open_serial_failure);
  RUN_TEST(test_create_closes_device_on_failure);
  RUN_TEST(test_create_get_samplerates_second_call_failure);
  RUN_TEST(test_create_no_sample_rates);
  RUN_TEST(test_create_bias_tee);
  RUN_TEST(test_create_invalid_settings_do_not_open_device);
  RUN_TEST(test_create_max_gains_are_valid);
  RUN_TEST(test_create_zero_buffer_len);
  RUN_TEST(test_create_default_sample_rate);
  RUN_TEST(test_create_default_sample_rate_not_supported);
  RUN_TEST(test_create_default_sample_rate_write_failure);
  RUN_TEST(test_destroy_null);
  RUN_TEST(test_rx_auto_gain);
  RUN_TEST(test_rx_sensitivity_gain);
  RUN_TEST(test_rx_linearity_gain);
  RUN_TEST(test_rx_manual_gain);
  RUN_TEST(test_rx_combined_gain_boundaries);
  RUN_TEST(test_rx_frequency_boundaries);
  RUN_TEST(test_rx_invalid_parameters_do_not_touch_device);
  RUN_TEST(test_rx_sample_rates);
  RUN_TEST(test_rx_default_sample_rate_not_rewritten);
  RUN_TEST(test_rx_sample_rate_must_match_exactly);
  RUN_TEST(test_rx_device_specific_sample_rates);
  RUN_TEST(test_rx_write_failures);
  RUN_TEST(test_rx_sample_rate_retried_after_failure);
  RUN_TEST(test_rx_not_started);
  RUN_TEST(test_rx_success);
  RUN_TEST(test_rx_blocks_are_not_aligned_with_buffer);
  RUN_TEST(test_rx_ring_wrap_around);
  RUN_TEST(test_rx_overflow_drops_new_blocks);
  RUN_TEST(test_rx_block_larger_than_ring_is_dropped);
  RUN_TEST(test_rx_partial_block_is_never_written);
  RUN_TEST(test_rx_device_reported_dropped_samples);
  RUN_TEST(test_rx_unexpected_sample_type_is_dropped);
  RUN_TEST(test_rx_empty_and_negative_blocks);
  RUN_TEST(test_rx_timeout_and_device_stopped_streaming);
  RUN_TEST(test_rx_data_is_returned_even_if_device_stopped_streaming);
  RUN_TEST(test_rx_timeout_is_respected);
  RUN_TEST(test_rx_default_timeout);
  RUN_TEST(test_rx_waits_for_data);
  RUN_TEST(test_rx_stop_wakes_up_waiting_reader);
  RUN_TEST(test_rx_stop);
  RUN_TEST(test_rx_stop_without_start);
  RUN_TEST(test_rx_start_and_stop_are_idempotent);
  RUN_TEST(test_rx_restart_discards_old_data);
  RUN_TEST(test_rx_start_failure);
  RUN_TEST(test_rx_stop_failure);
  RUN_TEST(test_rx_data_after_stop_is_ignored);
  RUN_TEST(test_destroy_stops_rx_and_closes_device);
  RUN_TEST(test_destroy_not_started);
  RUN_TEST(test_tx_not_supported);
  return UNITY_END();
}
