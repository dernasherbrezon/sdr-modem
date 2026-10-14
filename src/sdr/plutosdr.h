#ifndef SDR_MODEM_PLUTOSDR_H
#define SDR_MODEM_PLUTOSDR_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdr_device.h"
#include "iio_lib.h"

typedef struct plutosdr_t plutosdr;

typedef enum {
  IIO_GAIN_MODE_MANUAL = 0,
  IIO_GAIN_MODE_FAST_ATTACK,
  IIO_GAIN_MODE_SLOW_ATTACK,
  IIO_GAIN_MODE_HYBRID
} iio_gain_mode;

// settings are only read during plutosdr_create and can be freed by the caller afterward
typedef struct {
  // completely disable TX when doing RX only. significantly improves RX sensitivity
  bool tx_powerdown;
  iio_gain_mode gain_control_mode;
  unsigned int timeout_ms; // timeout for all iio operations
  char *uri;
  uint64_t sample_rate;  // default sample rate
} plutosdr_settings;

int plutosdr_create(const plutosdr_settings *settings, uint32_t sdr_buffer_len, iio_lib *lib, sdr_device **result);

#endif //SDR_MODEM_PLUTOSDR_H
