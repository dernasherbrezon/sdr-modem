#include "file_source.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>
#include <zlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_2PI ((float) (2 * M_PI))

struct file_device_t {
  uint32_t id;

  FILE *rx_file;
  FILE *tx_file;
  gzFile rx_gz;
  gzFile tx_gz;

  int rx_format;
  int tx_format;

  float complex *temp;
  size_t temp_len;

  //raw on-disk bytes for formats that don't use float complex directly (e.g. cu8)
  uint8_t *raw_temp;
};

static bool has_gz_suffix(const char *filename) {
  size_t len = strlen(filename);
  return len > 3 && strcmp(filename + len - 3, ".gz") == 0;
}

static bool is_valid_file_format(int format) {
  return format == FILE_FORMAT_CU8 || format == FILE_FORMAT_CF32;
}

static void cu8_to_cf32(const uint8_t *raw, float complex *out, size_t nsamples) {
  for (size_t i = 0; i < nsamples; i++) {
    float re = ((float) raw[2 * i] - 127.5f) / 127.5f;
    float im = ((float) raw[2 * i + 1] - 127.5f) / 127.5f;
    out[i] = re + im * I;
  }
}

static uint8_t clamp_u8(float value) {
  if (value < 0.0f) {
    return 0;
  }
  if (value > 255.0f) {
    return 255;
  }
  return (uint8_t) value;
}

static void cf32_to_cu8(const float complex *in, uint8_t *raw, size_t nsamples) {
  for (size_t i = 0; i < nsamples; i++) {
    raw[2 * i] = clamp_u8(roundf(crealf(in[i]) * 127.5f + 127.5f));
    raw[2 * i + 1] = clamp_u8(roundf(cimagf(in[i]) * 127.5f + 127.5f));
  }
}

void file_source_stop(void *plugin) {
  //do nothing. file source is not blocking
}

int file_source_create(uint32_t id, const char *rx_filename, int rx_format, const char *tx_filename, int tx_format, uint64_t sample_rate, uint32_t max_output_buffer_length, sdr_device **output) {
  if ((rx_filename != NULL && !is_valid_file_format(rx_format)) || (tx_filename != NULL && !is_valid_file_format(tx_format))) {
    fprintf(stderr, "<3>[%d] unsupported file format\n", id);
    return -1;
  }
  struct file_device_t *device = malloc(sizeof(struct file_device_t));
  if (device == NULL) {
    return -ENOMEM;
  }
  *device = (struct file_device_t){0};
  device->id = id;
  device->rx_format = rx_format;
  device->tx_format = tx_format;
  device->temp_len = max_output_buffer_length;
  device->temp = malloc(sizeof(float complex) * device->temp_len);
  if (device->temp == NULL) {
    file_source_destroy(device);
    return -ENOMEM;
  }
  if (rx_format == FILE_FORMAT_CU8 || tx_format == FILE_FORMAT_CU8) {
    device->raw_temp = malloc(sizeof(uint8_t) * 2 * device->temp_len);
    if (device->raw_temp == NULL) {
      file_source_destroy(device);
      return -ENOMEM;
    }
  }

  if (rx_filename != NULL) {
    if (strcmp(rx_filename, "-") == 0) {
      device->rx_file = stdin;
    } else if (has_gz_suffix(rx_filename)) {
      device->rx_gz = gzopen(rx_filename, "rb");
      if (device->rx_gz == NULL) {
        fprintf(stderr, "<3>[%d] unable to open file for input: %s\n", device->id, rx_filename);
        file_source_destroy(device);
        return -1;
      }
    } else {
      device->rx_file = fopen(rx_filename, "rb");
      if (device->rx_file == NULL) {
        fprintf(stderr, "<3>[%d] unable to open file for input: %s\n", device->id, rx_filename);
        file_source_destroy(device);
        return -1;
      }
    }
  }

  if (tx_filename != NULL) {
    if (strcmp(tx_filename, "-") == 0) {
      device->tx_file = stdout;
    } else if (has_gz_suffix(tx_filename)) {
      device->tx_gz = gzopen(tx_filename, "wb");
      if (device->tx_gz == NULL) {
        fprintf(stderr, "<3>[%d] unable to open file for output: %s\n", device->id, tx_filename);
        file_source_destroy(device);
        return -1;
      }
    } else {
      device->tx_file = fopen(tx_filename, "wb");
      if (device->tx_file == NULL) {
        fprintf(stderr, "<3>[%d] unable to open file for output: %s\n", device->id, tx_filename);
        file_source_destroy(device);
        return -1;
      }
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
  size_t bytes_per_sample = (device->rx_format == FILE_FORMAT_CU8) ? (2 * sizeof(uint8_t)) : sizeof(float complex);
  void *read_buf = (device->rx_format == FILE_FORMAT_CU8) ? (void *) device->raw_temp : (void *) device->temp;
  size_t actually_read;
  if (device->rx_gz != NULL) {
    int result = gzread(device->rx_gz, read_buf, (unsigned int) (bytes_per_sample * device->temp_len));
    if (result <= 0) {
      *output = NULL;
      *output_len = 0;
      return -1;
    }
    actually_read = (size_t) result / bytes_per_sample;
  } else if (device->rx_file != NULL) {
    actually_read = fread(read_buf, bytes_per_sample, device->temp_len, device->rx_file);
    if (actually_read == 0) {
      *output = NULL;
      *output_len = 0;
      return -1;
    }
  } else {
    fprintf(stderr, "<3>[%d] rx file was not initialized\n", device->id);
    *output = NULL;
    *output_len = 0;
    return -1;
  }
  if (device->rx_format == FILE_FORMAT_CU8) {
    cu8_to_cf32(device->raw_temp, device->temp, actually_read);
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
  const void *write_buf;
  size_t bytes_per_sample;
  if (device->tx_format == FILE_FORMAT_CU8) {
    cf32_to_cu8(input, device->raw_temp, input_len);
    write_buf = device->raw_temp;
    bytes_per_sample = 2 * sizeof(uint8_t);
  } else {
    write_buf = input;
    bytes_per_sample = sizeof(float complex);
  }
  //ignore actually written
  if (device->tx_gz != NULL) {
    gzwrite(device->tx_gz, write_buf, (unsigned int) (bytes_per_sample * input_len));
  } else if (device->tx_file != NULL) {
    fwrite(write_buf, bytes_per_sample, input_len, device->tx_file);
  } else {
    fprintf(stderr, "<3>[%d] tx file was not initialized\n", device->id);
    return -1;
  }
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
  if (device->rx_gz != NULL) {
    gzclose(device->rx_gz);
  }
  if (device->tx_gz != NULL) {
    gzclose(device->tx_gz);
  }
  if (device->temp != NULL) {
    free(device->temp);
  }
  if (device->raw_temp != NULL) {
    free(device->raw_temp);
  }
  free(device);
}
