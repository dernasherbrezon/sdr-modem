#ifndef SDR_MODEM_IIO_LIB_MOCK_H
#define SDR_MODEM_IIO_LIB_MOCK_H

#include "../src/sdr/iio_lib.h"

#include <stdbool.h>

// what the code under test wrote into the mocked device
typedef struct {
  int fir_config_writes;
  char fir_config[8192];
  int fir_enable_calls;
  bool fir_enabled;
  // index 0 - rx, 1 - tx
  long long sampling_frequency[2];
  long long rf_bandwidth[2];
  double hardwaregain[2];
  int hardwaregain_writes[2];
  int sampling_frequency_writes;
  // rx and tx local oscillators are the same object in the mock
  long long frequency;
  int frequency_writes;
  // last written value
  long long powerdown;
  int powerdown_writes;
  char gain_control_mode[32];
  char calib_mode[32];
  // value of sampling_frequency_writes at the moment calib_mode was written
  int calib_mode_sampling_frequency_writes;
} iio_lib_mock_stats;

int iio_lib_mock_create(int16_t *expected_rx, size_t expected_rx_len, int16_t *expected_tx, iio_lib **lib);

void iio_lib_mock_get_tx(int16_t **output, size_t *output_len);

// restore defaults: typical ad9361 ranges, current sample rate 1MSPS, no failures. called by iio_lib_mock_create
void iio_lib_mock_reset(void);

// override value returned by iio_channel_attr_read. output - tx channel (true) or rx channel (false)
void iio_lib_mock_set_attr(bool output, const char *name, const char *value);

void iio_lib_mock_set_current_sample_rate(long long sample_rate);

// make any write of the attribute fail with -EIO. NULL - stop failing
void iio_lib_mock_fail_write(const char *attr);

const iio_lib_mock_stats *iio_lib_mock_get_stats(void);

void iio_lib_mock_destroy(iio_lib *lib);

#endif //SDR_MODEM_IIO_LIB_MOCK_H
