#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdr/iio_lib.h"
#include "dsp/gfsk_modem.h"
#include "dsp/bpsk_modem.h"
#include "dsp/psk_pm_modem.h"
#include "dsp/sdr_modem.h"
#include "sdr/file_source.h"
#include "sdr/sdr_server_client.h"

typedef enum {
  // returned when a sdr type name cannot be parsed
  SDR_TYPE_INVALID = -1,
  SDR_TYPE_NONE = 0,
  SDR_TYPE_PLUTOSDR,
  SDR_TYPE_FILE,
  SDR_TYPE_SDR_SERVER
} sdr_device_type;

#define FRAMING_TYPE_NONE 0

// direction is only meaningful in cli mode (bind_address == NULL): it selects whether the
// single configured sdr/modem pipeline demodulates (rx) or modulates (tx). server mode ignores
// it and dispatches based on the client's RxRequest/TxRequest instead.
#define DIRECTION_RX 1
#define DIRECTION_TX 2

typedef struct {
  // socket settings
  char *bind_address;
  uint16_t port;
  int read_timeout_seconds;

  uint32_t buffer_size;
  uint16_t queue_size;

  int direction;

  sdr_device_type sdr_type;

  sdr_server_settings sdr_server;

  sdr_file_settings sdr_file;

  double plutosdr_gain;
  unsigned int plutosdr_timeout_millis;
  iio_lib *iio;

  char *input_file;
  char *output_file;

  sdr_modem_type modem;
  int framing;
  uint64_t frequency;
  uint64_t sample_rate;
  // settings of each modem type are kept separately, the active one is selected by "modem"
  gfsk_modem_settings gfsk;
  // bpsk, dpsk and sdpsk share the same settings. psk.type is derived from "modem"
  bpsk_modem_settings psk;
  psk_pm_modem_settings psk_pm;
  char *freq_offset_file;
  char *debug_freq_offset_file;
  char *debug_constellation_file;
  char *debug_baseband_file;
  char *debug_subcarrier_file;

} app_config;

int app_config_create(int argc, char **argv, app_config **config);

void app_config_destroy(app_config *config);

#endif /* APP_CONFIG_H_ */
