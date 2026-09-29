#ifndef SDR_MODEM_FILE_SOURCE_H
#define SDR_MODEM_FILE_SOURCE_H

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
} file_source_format;

int file_source_create(uint32_t id, const char *rx_filename, file_source_format rx_format, const char *tx_filename, file_source_format tx_format, uint64_t sample_rate, uint32_t max_output_buffer_length, sdr_device **result);

int file_source_process_rx(float complex **output, size_t *output_len, void *plugin);

int file_source_process_tx(float complex *input, size_t input_len, void *plugin);

void file_source_destroy(void *plugin);

#endif //SDR_MODEM_FILE_SOURCE_H
