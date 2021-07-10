#ifndef SDR_MODEM_SDR_DEVICE_H
#define SDR_MODEM_SDR_DEVICE_H

#include <complex.h>
#include <stdlib.h>

typedef struct sdr_device_t sdr_device;

struct sdr_device_t {
    void *plugin;

    int (*sdr_process_rx)(float complex **output, size_t *output_len, void *plugin);
    int (*sdr_process_tx)(float complex *input, size_t input_len, void *plugin);
    void (*destroy)(void *plugin);
    void (*stop_rx)(void *plugin);
};

#endif //SDR_MODEM_SDR_DEVICE_H
