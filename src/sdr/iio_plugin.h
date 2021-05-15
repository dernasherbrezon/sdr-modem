#ifndef SDR_MODEM_IIO_PLUGIN_H
#define SDR_MODEM_IIO_PLUGIN_H

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct iio_plugin_t iio_plugin;

struct stream_cfg {
    long long bw_hz; // Analog bandwidth in Hz
    long long fs_hz; // Baseband sample rate in Hz
    long long center_freq; // Local oscillator frequency in Hz
    const char *rfport; // Port name
};

int iio_plugin_create(uint32_t id, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_plugin **result);

void iio_plugin_process_rx(float complex **output, size_t *output_len, iio_plugin *iio);

void iio_plugin_destroy(iio_plugin *iio);

#endif //SDR_MODEM_IIO_PLUGIN_H
