#include "file_source.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <liquid/liquid.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_2PI ((float) (2 * M_PI))

struct file_device_t {
  uint32_t id;

  FILE *rx_file;
  FILE *tx_file;

  float complex *temp;
  size_t temp_len;
};

void file_source_stop(void *plugin) {
  //do nothing. file source is not blocking
}

int file_source_create(uint32_t id, const char *rx_filename, const char *tx_filename, uint64_t sample_rate, uint32_t max_output_buffer_length, sdr_device **output) {
  struct file_device_t *device = malloc(sizeof(struct file_device_t));
  if (device == NULL) {
    return -ENOMEM;
  }
  *device = (struct file_device_t){0};
  device->id = id;
  device->temp_len = max_output_buffer_length;
  device->temp = malloc(sizeof(float complex) * device->temp_len);
  if (device->temp == NULL) {
    file_source_destroy(device);
    return -ENOMEM;
  }

  if (rx_filename != NULL) {
    device->rx_file = fopen(rx_filename, "rb");
    if (device->rx_file == NULL) {
      fprintf(stderr, "<3>[%d] unable to open file for input: %s\n", device->id, rx_filename);
      file_source_destroy(device);
      return -1;
    }
  }

  if (tx_filename != NULL) {
    device->tx_file = fopen(tx_filename, "wb");
    if (device->tx_file == NULL) {
      fprintf(stderr, "<3>[%d] unable to open file for output: %s\n", device->id, tx_filename);
      file_source_destroy(device);
      return -1;
    }
  }

  struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
  if (result == NULL) {
    file_source_destroy(device);
    return -ENOMEM;
  }
  result->plugin = device;
  result->destroy = file_source_destroy;
  result->sdr_process_rx = file_source_process_rx;
  result->sdr_process_tx = file_source_process_tx;
  result->stop_rx = file_source_stop;

  *output = result;
  return 0;
}

int file_source_process_rx(float complex **output, size_t *output_len, void *plugin) {
  file_device *device = (file_device *) plugin;
  if (device->rx_file == NULL) {
    fprintf(stderr, "<3>[%d] rx file was not initialized\n", device->id);
    *output = NULL;
    *output_len = 0;
    return -1;
  }
  size_t actually_read = fread(device->temp, sizeof(float complex), device->temp_len, device->rx_file);
  if (actually_read == 0) {
    *output = NULL;
    *output_len = 0;
    return -1;
  }
  *output = device->temp;
  *output_len = actually_read;
  return 0;
}

int file_source_process_tx(float complex *input, size_t input_len, void *plugin) {
  file_device *device = (file_device *) plugin;
  if (input_len > device->temp_len) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, device->temp_len);
    return -1;
  }
  if (device->tx_file == NULL) {
    fprintf(stderr, "<3>[%d] tx file was not initialized\n", device->id);
    return -1;
  }
  //ignore actually written
  fwrite(input, sizeof(float complex), input_len, device->tx_file);
  return 0;
}

void file_source_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  file_device *device = (file_device *) plugin;
  if (device->rx_file != NULL) {
    fclose(device->rx_file);
  }
  if (device->tx_file != NULL) {
    fclose(device->tx_file);
  }
  if (device->temp != NULL) {
    free(device->temp);
  }
  free(device);
}
