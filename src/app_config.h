#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdr/iio_lib.h"
#include "api.pb-c.h"

#define SDR_TYPE_NONE 1
#define SDR_TYPE_PLUTOSDR 2
#define SDR_TYPE_FILE 3
#define SDR_TYPE_SDR_SERVER 4

#define MODEM_TYPE_NONE 0
#define MODEM_TYPE_GFSK 1

#define FRAMING_TYPE_NONE 0

typedef struct {
  // socket settings
  char *bind_address;
  uint16_t port;
  int read_timeout_seconds;

  uint32_t buffer_size;
  uint16_t queue_size;

  int rx_sdr_type;
  char *rx_sdr_server_address;
  int rx_sdr_server_port;

  char *rx_file;
  char *tx_file;

  int tx_sdr_type;
  double tx_plutosdr_gain;
  double rx_plutosdr_gain;
  unsigned int tx_plutosdr_timeout_millis;
  iio_lib *iio;

  char *input_file;
  char *output_file;

  int rx_modem;
  int rx_framing;
  struct ModemRequest rx_req;
  char *rx_freq_offset_file;
  char *rx_debug_freq_offset_file;

  int tx_modem;
  int tx_framing;
  struct ModemRequest tx_req;
  char *tx_freq_offset_file;
  char *tx_debug_freq_offset_file;

} app_config;

int app_config_create(int argc, char **argv, app_config **config);

void app_config_destroy(app_config *config);

#endif /* APP_CONFIG_H_ */
