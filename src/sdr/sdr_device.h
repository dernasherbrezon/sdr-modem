#ifndef SDR_MODEM_SDR_DEVICE_H
#define SDR_MODEM_SDR_DEVICE_H

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct sdr_device_t sdr_device;

typedef struct {
  uint64_t frequency;
  uint64_t sample_rate;
  float gain;
} sdr_channel_config;

struct sdr_device_t {
  void *plugin;

  int (*sdr_process_rx)(float complex **output, size_t *output_len, void *plugin);

  int (*sdr_process_tx)(float complex *input, size_t input_len, void *plugin);

  void (*destroy)(void *plugin);

  int (*set_rx_parameters)(sdr_channel_config *config, void *plugin);

  int (*set_tx_parameters)(sdr_channel_config *config, void *plugin);

  void (*start_rx)(void *plugin);

  void (*stop_rx)(void *plugin);
};

#endif //SDR_MODEM_SDR_DEVICE_H
