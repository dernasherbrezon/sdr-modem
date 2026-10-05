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
} plutosdr_settings;

int plutosdr_create(uint32_t id, const plutosdr_settings *settings, uint32_t max_input_buffer_length, iio_lib *lib, sdr_device **result);

int plutosdr_process_rx(float complex **output, size_t *output_len, void *plugin);

int plutosdr_process_tx(float complex *input, size_t input_len, void *plugin);

int plutosdr_set_rx_parameters(sdr_channel_config *config, void *plugin);

int plutosdr_set_tx_parameters(sdr_channel_config *config, void *plugin);

void plutosdr_destroy(void *plugin);

#endif //SDR_MODEM_PLUTOSDR_H
