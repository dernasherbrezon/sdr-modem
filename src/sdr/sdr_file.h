#ifndef SDR_MODEM_SDR_FILE_H
#define SDR_MODEM_SDR_FILE_H

#include "sdr_device.h"

typedef struct file_device_t file_device;

typedef enum {
  // returned when a file format cannot be parsed or guessed
  FILE_FORMAT_INVALID = -1,
  // detect format from the file extension
  FILE_FORMAT_GUESS = 0,
  FILE_FORMAT_CU8,
  FILE_FORMAT_CF32,
  FILE_FORMAT_CS16
} sdr_file_format;

// file source acts as an sdr with both rx and tx capabilities: rx reads i/q samples from rx_file,
// tx writes i/q samples to tx_file. set the file to NULL if the corresponding direction is not used.
// "-" means stdin (rx) or stdout (tx). ".gz" suffix enables gzip
typedef struct {
  char *rx_file;                     // file to read i/q samples from
  sdr_file_format rx_file_format; // format of the i/q samples in rx_file
  char *tx_file;                     // file to write i/q samples to
  sdr_file_format tx_file_format; // format of the i/q samples in tx_file
} sdr_file_settings;

int sdr_file_create(const sdr_file_settings *settings, uint32_t max_output_buffer_length, sdr_device **result);

int sdr_file_process_rx(float complex **output, size_t *output_len, void *plugin);

int sdr_file_process_tx(float complex *input, size_t input_len, void *plugin);

void sdr_file_destroy(void *plugin);

#endif //SDR_MODEM_SDR_FILE_H
