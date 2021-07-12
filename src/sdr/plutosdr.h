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

struct stream_cfg {
    uint32_t sampling_freq; // Baseband sample rate in Hz
    uint32_t center_freq; // Local oscillator frequency in Hz
    uint8_t gain_control_mode;
    double manual_gain;
};

int plutosdr_create(uint32_t id, bool rx_only, struct stream_cfg *rx_config, struct stream_cfg *tx_config, unsigned int timeout_ms, uint32_t max_input_buffer_length, iio_lib *lib, sdr_device **result);

int plutosdr_process_rx(float complex **output, size_t *output_len, void *plugin);

int plutosdr_process_tx(float complex *input, size_t input_len, void *plugin);

void plutosdr_destroy(void *plugin);

#endif //SDR_MODEM_PLUTOSDR_H
