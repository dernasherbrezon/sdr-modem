#ifndef SDR_MODEM_SDR_DEVICE_H
#define SDR_MODEM_SDR_DEVICE_H

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  uint64_t frequency;
  uint64_t sample_rate;
  float gain;
} sdr_channel_config;

typedef struct {
  void *plugin;

  void (*sdr_set_rx_callback)(void (*callback)(float complex *input, size_t input_len, void *ctx), void *ctx, void *plugin);

  int (*sdr_process_rx)(float complex **output, size_t *output_len, void *plugin);

  int (*sdr_process_tx)(float complex *input, size_t input_len, void *plugin);

  // Ensure last transmitted data was sent over the air
  void (*sdr_flush_tx)(void *plugin);

  void (*destroy)(void *plugin);

  int (*set_rx_parameters)(sdr_channel_config *config, void *plugin);

  int (*set_tx_parameters)(sdr_channel_config *config, void *plugin);

  void (*start_rx)(void *plugin);

  void (*stop_rx)(void *plugin);
} sdr_device;

#endif //SDR_MODEM_SDR_DEVICE_H
