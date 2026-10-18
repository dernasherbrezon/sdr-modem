#include "airspy_lib_mock.h"
#include <stdlib.h>
#include <string.h>

#define MOCK_ERROR_OTHER (-9999)

struct airspy_device {
  uint8_t dummy;
};

static struct airspy_device mock_device;
static airspy_lib_mock_stats mock_stats;
static uint32_t mock_sample_rates[8];
static uint32_t mock_sample_rates_count;
static char mock_fail_function[64];
static char mock_fail_on_call_function[64];
static int mock_fail_on_call_n;
static int mock_fail_on_call_seen;
static bool mock_streaming;
static airspy_lib_sample_block_cb_fn mock_callback;
static void *mock_callback_ctx;

static bool mock_should_fail(const char *function) {
  if (mock_fail_function[0] != '\0' && strcmp(mock_fail_function, function) == 0) {
    return true;
  }
  if (mock_fail_on_call_function[0] != '\0' && strcmp(mock_fail_on_call_function, function) == 0) {
    mock_fail_on_call_seen++;
    return mock_fail_on_call_seen == mock_fail_on_call_n;
  }
  return false;
}

#define MOCK_FAIL_IF_REQUESTED(name) \
  if (mock_should_fail(name)) { \
    return MOCK_ERROR_OTHER; \
  }

static int mock_open(struct airspy_device **device) {
  MOCK_FAIL_IF_REQUESTED("airspy_open")
  mock_stats.open_calls++;
  *device = &mock_device;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_open_sn(struct airspy_device **device, uint64_t serial_number) {
  MOCK_FAIL_IF_REQUESTED("airspy_open_sn")
  mock_stats.open_sn_calls++;
  mock_stats.serial_number = serial_number;
  *device = &mock_device;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_close(struct airspy_device *device) {
  mock_stats.close_calls++;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_get_samplerates(struct airspy_device *device, uint32_t *buffer, uint32_t len) {
  MOCK_FAIL_IF_REQUESTED("airspy_get_samplerates")
  if (len == 0) {
    buffer[0] = mock_sample_rates_count;
    return AIRSPY_LIB_SUCCESS;
  }
  for (uint32_t i = 0; i < len && i < mock_sample_rates_count; i++) {
    buffer[i] = mock_sample_rates[i];
  }
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_samplerate(struct airspy_device *device, uint32_t samplerate) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_samplerate")
  mock_stats.sample_rate_writes++;
  mock_stats.sample_rate = samplerate;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_sample_type(struct airspy_device *device, int sample_type) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_sample_type")
  mock_stats.sample_type_calls++;
  mock_stats.sample_type = sample_type;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_start_rx(struct airspy_device *device, airspy_lib_sample_block_cb_fn callback, void *rx_ctx) {
  MOCK_FAIL_IF_REQUESTED("airspy_start_rx")
  mock_stats.start_rx_calls++;
  mock_callback = callback;
  mock_callback_ctx = rx_ctx;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_stop_rx(struct airspy_device *device) {
  MOCK_FAIL_IF_REQUESTED("airspy_stop_rx")
  mock_stats.stop_rx_calls++;
  mock_callback = NULL;
  mock_callback_ctx = NULL;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_is_streaming(struct airspy_device *device) {
  return mock_streaming ? AIRSPY_LIB_TRUE : 0;
}

static int mock_set_freq(struct airspy_device *device, uint32_t freq_hz) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_freq")
  mock_stats.freq_writes++;
  mock_stats.freq = freq_hz;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_lna_gain(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_lna_gain")
  mock_stats.lna_gain_writes++;
  mock_stats.lna_gain = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_mixer_gain(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_mixer_gain")
  mock_stats.mixer_gain_writes++;
  mock_stats.mixer_gain = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_vga_gain(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_vga_gain")
  mock_stats.vga_gain_writes++;
  mock_stats.vga_gain = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_lna_agc(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_lna_agc")
  mock_stats.lna_agc_writes++;
  mock_stats.lna_agc = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_mixer_agc(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_mixer_agc")
  mock_stats.mixer_agc_writes++;
  mock_stats.mixer_agc = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_linearity_gain(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_linearity_gain")
  mock_stats.linearity_gain_writes++;
  mock_stats.linearity_gain = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_sensitivity_gain(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_sensitivity_gain")
  mock_stats.sensitivity_gain_writes++;
  mock_stats.sensitivity_gain = value;
  return AIRSPY_LIB_SUCCESS;
}

static int mock_set_rf_bias(struct airspy_device *device, uint8_t value) {
  MOCK_FAIL_IF_REQUESTED("airspy_set_rf_bias")
  mock_stats.rf_bias_writes++;
  mock_stats.rf_bias = value;
  return AIRSPY_LIB_SUCCESS;
}

static const char *mock_error_name(int errcode) {
  return "mock error";
}

static void mock_reset(void) {
  memset(&mock_stats, 0, sizeof(mock_stats));
  mock_sample_rates[0] = 2500000;
  mock_sample_rates[1] = 10000000;
  mock_sample_rates_count = 2;
  mock_fail_function[0] = '\0';
  mock_fail_on_call_function[0] = '\0';
  mock_fail_on_call_n = 0;
  mock_fail_on_call_seen = 0;
  mock_streaming = true;
  mock_callback = NULL;
  mock_callback_ctx = NULL;
}

int airspy_lib_mock_create(airspy_lib **lib) {
  mock_reset();
  struct airspy_lib_t *result = malloc(sizeof(struct airspy_lib_t));
  if (result == NULL) {
    return -1;
  }
  *result = (struct airspy_lib_t) {0};
  result->airspy_open = mock_open;
  result->airspy_open_sn = mock_open_sn;
  result->airspy_close = mock_close;
  result->airspy_get_samplerates = mock_get_samplerates;
  result->airspy_set_samplerate = mock_set_samplerate;
  result->airspy_set_sample_type = mock_set_sample_type;
  result->airspy_start_rx = mock_start_rx;
  result->airspy_stop_rx = mock_stop_rx;
  result->airspy_is_streaming = mock_is_streaming;
  result->airspy_set_freq = mock_set_freq;
  result->airspy_set_lna_gain = mock_set_lna_gain;
  result->airspy_set_mixer_gain = mock_set_mixer_gain;
  result->airspy_set_vga_gain = mock_set_vga_gain;
  result->airspy_set_lna_agc = mock_set_lna_agc;
  result->airspy_set_mixer_agc = mock_set_mixer_agc;
  result->airspy_set_linearity_gain = mock_set_linearity_gain;
  result->airspy_set_sensitivity_gain = mock_set_sensitivity_gain;
  result->airspy_set_rf_bias = mock_set_rf_bias;
  result->airspy_error_name = mock_error_name;
  *lib = result;
  return 0;
}

void airspy_lib_mock_destroy(airspy_lib *lib) {
  free(lib);
}

const airspy_lib_mock_stats *airspy_lib_mock_get_stats(void) {
  return &mock_stats;
}

void airspy_lib_mock_set_sample_rates(const uint32_t *rates, uint32_t count) {
  mock_sample_rates_count = count;
  for (uint32_t i = 0; i < count && i < sizeof(mock_sample_rates) / sizeof(mock_sample_rates[0]); i++) {
    mock_sample_rates[i] = rates[i];
  }
}

void airspy_lib_mock_fail(const char *function) {
  if (function == NULL) {
    mock_fail_function[0] = '\0';
    return;
  }
  strncpy(mock_fail_function, function, sizeof(mock_fail_function) - 1);
}

void airspy_lib_mock_fail_on_call(const char *function, int n) {
  strncpy(mock_fail_on_call_function, function, sizeof(mock_fail_on_call_function) - 1);
  mock_fail_on_call_n = n;
  mock_fail_on_call_seen = 0;
}

void airspy_lib_mock_set_streaming(bool streaming) {
  mock_streaming = streaming;
}

void airspy_lib_mock_deliver(const float complex *samples, int sample_count, uint64_t dropped_samples, int sample_type) {
  airspy_lib_sample_block_cb_fn callback = mock_callback;
  if (callback == NULL) {
    return;
  }
  airspy_lib_transfer transfer = {
      .device = &mock_device,
      .ctx = mock_callback_ctx,
      .samples = (void *) samples,
      .sample_count = sample_count,
      .dropped_samples = dropped_samples,
      .sample_type = sample_type,
  };
  callback(&transfer);
}
