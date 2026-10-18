#ifndef SDR_MODEM_AIRSPY_LIB_MOCK_H
#define SDR_MODEM_AIRSPY_LIB_MOCK_H

#include <complex.h>
#include <stdbool.h>
#include "../src/sdr/airspy_lib.h"

// what the code under test did with the mocked device
typedef struct {
  int open_calls;
  int open_sn_calls;
  uint64_t serial_number;
  int close_calls;

  int sample_type_calls;
  int sample_type;

  int sample_rate_writes;
  uint32_t sample_rate;

  int freq_writes;
  uint32_t freq;

  int rf_bias_writes;
  int rf_bias;

  int lna_agc_writes;
  int lna_agc;
  int mixer_agc_writes;
  int mixer_agc;

  int lna_gain_writes;
  int lna_gain;
  int mixer_gain_writes;
  int mixer_gain;
  int vga_gain_writes;
  int vga_gain;
  int sensitivity_gain_writes;
  int sensitivity_gain;
  int linearity_gain_writes;
  int linearity_gain;

  int start_rx_calls;
  int stop_rx_calls;
} airspy_lib_mock_stats;

// restores defaults: sample rates 2.5M and 10M, no failures, streaming
int airspy_lib_mock_create(airspy_lib **lib);

void airspy_lib_mock_destroy(airspy_lib *lib);

const airspy_lib_mock_stats *airspy_lib_mock_get_stats(void);

void airspy_lib_mock_set_sample_rates(const uint32_t *rates, uint32_t count);

// the function (e.g. "airspy_set_freq") returns an error. NULL - stop failing
void airspy_lib_mock_fail(const char *function);

// the function fails only on its n-th call (1-based). useful when the same function is called several times
void airspy_lib_mock_fail_on_call(const char *function, int n);

// value returned by airspy_is_streaming
void airspy_lib_mock_set_streaming(bool streaming);

// emulates libairspy streaming thread: calls the registered callback in the caller's thread.
// does nothing if rx was not started
void airspy_lib_mock_deliver(const float complex *samples, int sample_count, uint64_t dropped_samples, int sample_type);

#endif //SDR_MODEM_AIRSPY_LIB_MOCK_H
