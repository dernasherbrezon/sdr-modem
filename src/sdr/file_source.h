#ifndef SDR_MODEM_FILE_SOURCE_H
#define SDR_MODEM_FILE_SOURCE_H

#include "sdr_device.h"

typedef struct file_device_t file_device;

int file_source_create(uint32_t id, const char *rx_filename, const char *tx_filename, uint64_t sampling_freq, int64_t freq_offset, uint32_t max_output_buffer_length, sdr_device **result);

int file_source_process_rx(float complex **output, size_t *output_len, void *plugin);

int file_source_process_tx(float complex *input, size_t input_len, void *plugin);

void file_source_destroy(void *plugin);

#endif //SDR_MODEM_FILE_SOURCE_H
