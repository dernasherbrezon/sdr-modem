#ifndef SDR_MODEM_AIRSPY_H
#define SDR_MODEM_AIRSPY_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdr_device.h"
#include "airspy_lib.h"

typedef struct airspy_t airspy;

typedef enum {
  // LNA and mixer AGC are enabled, VGA gain is fixed to airspy_settings.vga_gain
  AIRSPY_GAIN_MODE_AUTO = 0,
  // airspy "sensitivity" gain table: sdr_channel_config.gain in range [0, 21]
  AIRSPY_GAIN_MODE_SENSITIVITY,
  // airspy "linearity" gain table: sdr_channel_config.gain in range [0, 21]
  AIRSPY_GAIN_MODE_LINEARITY,
  // LNA, mixer and VGA are fixed to airspy_settings.*_gain. sdr_channel_config.gain is ignored
  AIRSPY_GAIN_MODE_MANUAL
} airspy_gain_mode;

#define AIRSPY_MAX_LNA_GAIN 14
#define AIRSPY_MAX_MIXER_GAIN 15
#define AIRSPY_MAX_VGA_GAIN 15
#define AIRSPY_MAX_COMBINED_GAIN 21

// settings are only read during airspy_create and can be freed by the caller afterward
typedef struct {
  airspy_gain_mode gain_mode;
  // used only in the manual mode
  uint8_t lna_gain;
  // used only in the manual mode
  uint8_t mixer_gain;
  // used in the auto and manual modes
  uint8_t vga_gain;
  bool bias_tee;
  // 0 - use the first available device
  uint64_t serial_number;
  // default sample rate. 0 - leave as is
  uint64_t sample_rate;
} airspy_settings;

int airspy_create(const airspy_settings *settings, uint32_t sdr_buffer_len, airspy_lib *lib, sdr_device **result);

#endif //SDR_MODEM_AIRSPY_H
