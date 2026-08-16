#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>
#include "sdr/iio_lib.h"

#define RX_SDR_TYPE_SDR_SERVER 0
#define RX_SDR_TYPE_PLUTOSDR 1

#define TX_SDR_TYPE_NONE 0
#define TX_SDR_TYPE_PLUTOSDR 1

typedef struct {
  // socket settings
  char *bind_address;
  uint16_t port;
  int read_timeout_seconds;

  uint32_t buffer_size;
  uint16_t queue_size;

  uint8_t rx_sdr_type;
  char *rx_sdr_server_address;
  int rx_sdr_server_port;

  // output settings
  char *base_path;

  char *rx_file_base_path;
  char *tx_file_base_path;

  uint8_t tx_sdr_type;
  double tx_plutosdr_gain;
  double rx_plutosdr_gain;
  unsigned int tx_plutosdr_timeout_millis;
  iio_lib *iio;
} app_config;

int app_config_create(const char *path, app_config **config);

void app_config_destroy(app_config *config);

#endif /* APP_CONFIG_H_ */
