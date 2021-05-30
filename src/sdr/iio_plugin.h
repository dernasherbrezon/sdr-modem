#ifndef SDR_MODEM_IIO_PLUGIN_H
#define SDR_MODEM_IIO_PLUGIN_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdr_device.h"
#include "iio_lib.h"

typedef struct iio_plugin_t iio_plugin;

#define IIO_GAIN_MODE_MANUAL 0
#define IIO_GAIN_MODE_FAST_ATTACK 1
#define IIO_GAIN_MODE_SLOW_ATTACK 2
#define IIO_GAIN_MODE_HYBRID 3

struct stream_cfg {
    uint32_t sampling_freq; // Baseband sample rate in Hz
    uint32_t center_freq; // Local oscillator frequency in Hz
    uint8_t gain_control_mode;
    double manual_gain;
};

int iio_plugin_create(uint32_t id, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_lib *lib, sdr_device **result);

void iio_plugin_process_rx(float complex **output, size_t *output_len, void *plugin);
int iio_plugin_process_tx(float complex *input, size_t input_len, void *plugin);

void iio_plugin_destroy(void *plugin);

#endif //SDR_MODEM_IIO_PLUGIN_H
