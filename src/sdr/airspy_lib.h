#ifndef SDR_MODEM_AIRSPY_LIB_H
#define SDR_MODEM_AIRSPY_LIB_H

#include <stdint.h>
#include <stddef.h>

// libairspy is loaded at runtime (dlopen), the same way as libiio. This keeps it an optional dependency:
// neither libairspy nor its headers are needed to build sdr-modem.
// The declarations below mirror the stable public ABI of airspy.h

typedef struct airspy_lib_t airspy_lib;

struct airspy_device;

#define AIRSPY_LIB_SUCCESS 0
#define AIRSPY_LIB_TRUE 1
// airspy_sample_type
#define AIRSPY_LIB_SAMPLE_FLOAT32_IQ 0

typedef struct {
  struct airspy_device *device;
  void *ctx;
  void *samples;
  int sample_count;
  uint64_t dropped_samples;
  int sample_type;
} airspy_lib_transfer;

typedef int (*airspy_lib_sample_block_cb_fn)(airspy_lib_transfer *transfer);

struct airspy_lib_t {
  void *handle;

  int (*airspy_open)(struct airspy_device **device);

  int (*airspy_open_sn)(struct airspy_device **device, uint64_t serial_number);

  int (*airspy_close)(struct airspy_device *device);

  int (*airspy_get_samplerates)(struct airspy_device *device, uint32_t *buffer, uint32_t len);

  int (*airspy_set_samplerate)(struct airspy_device *device, uint32_t samplerate);

  int (*airspy_set_sample_type)(struct airspy_device *device, int sample_type);

  int (*airspy_start_rx)(struct airspy_device *device, airspy_lib_sample_block_cb_fn callback, void *rx_ctx);

  int (*airspy_stop_rx)(struct airspy_device *device);

  int (*airspy_is_streaming)(struct airspy_device *device);

  int (*airspy_set_freq)(struct airspy_device *device, uint32_t freq_hz);

  int (*airspy_set_lna_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_mixer_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_vga_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_lna_agc)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_mixer_agc)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_linearity_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_sensitivity_gain)(struct airspy_device *device, uint8_t value);

  int (*airspy_set_rf_bias)(struct airspy_device *device, uint8_t value);

  const char *(*airspy_error_name)(int errcode);
};

// fails if libairspy is not installed
int airspy_lib_create(airspy_lib **lib);

void airspy_lib_destroy(airspy_lib *lib);

#endif //SDR_MODEM_AIRSPY_LIB_H
