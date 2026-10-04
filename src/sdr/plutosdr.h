#ifndef SDR_MODEM_PLUTOSDR_H
#define SDR_MODEM_PLUTOSDR_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdr_device.h"
#include "iio_lib.h"

typedef struct plutosdr_t plutosdr;

#define IIO_GAIN_MODE_MANUAL 0
#define IIO_GAIN_MODE_FAST_ATTACK 1
#define IIO_GAIN_MODE_SLOW_ATTACK 2
#define IIO_GAIN_MODE_HYBRID 3

// settings are only read during plutosdr_create and can be freed by the caller afterward
typedef struct {
  // completely disable TX when doing RX only. significantly improves RX sensitivity
  bool tx_powerdown;
  uint64_t rx_sampling_frequency;      // baseband sample rate in Hz. 0 if rx is not used
  uint64_t rx_frequency;      // local oscillator frequency in Hz
  uint8_t rx_gain_control_mode; // one of IIO_GAIN_MODE_*
  double rx_hardwaregain;        // used only if rx_gain_control_mode is IIO_GAIN_MODE_MANUAL
  uint64_t tx_sampling_frequency;      // baseband sample rate in Hz. 0 if tx is not used
  uint64_t tx_frequency;      // local oscillator frequency in Hz
  double tx_hardwaregain;        // gain control mode is not applicable to TX: hardwaregain is always set manually
  unsigned int timeout_ms;      // timeout for all iio operations
} plutosdr_settings;

int plutosdr_create(uint32_t id, const plutosdr_settings *settings, uint32_t max_input_buffer_length, iio_lib *lib, sdr_device **result);

int plutosdr_process_rx(float complex **output, size_t *output_len, void *plugin);

int plutosdr_process_tx(float complex *input, size_t input_len, void *plugin);

void plutosdr_destroy(void *plugin);

#endif //SDR_MODEM_PLUTOSDR_H
