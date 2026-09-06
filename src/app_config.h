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
#define MODEM_TYPE_BPSK 2
#define MODEM_TYPE_DPSK 3
#define MODEM_TYPE_SDPSK 4
#define MODEM_TYPE_OQPSK 5
#define MODEM_TYPE_PSK_PM 6

#define FRAMING_TYPE_NONE 0

#define FILE_FORMAT_GUESS 0
#define FILE_FORMAT_CU8 1
#define FILE_FORMAT_CF32 2

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

  int sdr_type;
  char *sdr_server_address;
  int sdr_server_port;

  char *file;
  int file_format;

  double plutosdr_gain;
  unsigned int plutosdr_timeout_millis;
  iio_lib *iio;

  char *input_file;
  char *output_file;

  int modem;
  int framing;
  struct ModemRequest req;
  char *freq_offset_file;
  char *debug_freq_offset_file;
  char *debug_constellation_file;
  char *debug_baseband_file;

} app_config;

int app_config_create(int argc, char **argv, app_config **config);

void app_config_destroy(app_config *config);

#endif /* APP_CONFIG_H_ */
