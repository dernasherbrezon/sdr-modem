#ifndef SDR_MODEM_IIO_PLUGIN_H
#define SDR_MODEM_IIO_PLUGIN_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct iio_plugin_t iio_plugin;

#define IIO_GAIN_MODE_MANUAL 0
#define IIO_GAIN_MODE_FAST_ATTACK 1
#define IIO_GAIN_MODE_SLOW_ATTACK 2
#define IIO_GAIN_MODE_HYBRID 3

struct stream_cfg {
    uint32_t rf_bandwidth; // Analog bandwidth in Hz
    uint32_t sampling_freq; // Baseband sample rate in Hz
    uint32_t center_freq; // Local oscillator frequency in Hz
    uint8_t gain_control_mode;
    double manual_gain;
};

int iio_plugin_create(uint32_t id, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_plugin **result);

void iio_plugin_process_rx(float complex **output, size_t *output_len, iio_plugin *iio);
int iio_plugin_process_tx(float complex *input, size_t input_len, iio_plugin *iio);

void iio_plugin_destroy(iio_plugin *iio);

#endif //SDR_MODEM_IIO_PLUGIN_H
