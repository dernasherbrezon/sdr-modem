#include "iio_lib_mock.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

int16_t *iio_lib_mock_rx = NULL;
size_t iio_lib_mock_rx_len = 0;

int16_t *iio_lib_mock_tx = NULL;
size_t iio_lib_mock_tx_len_processed = 0;

struct iio_device *ad9361_phy = NULL;
struct iio_channel *iio_lib_mock_tx_channel = NULL;
struct iio_channel *iio_lib_mock_rx_channel = NULL;

#define MOCK_MAX_ATTRS 16

typedef struct {
  bool used;
  bool output;
  char name[64];
  char value[64];
} mock_attr;

static mock_attr mock_attrs[MOCK_MAX_ATTRS];
static long long mock_current_sample_rate = 0;
static char mock_fail_attr[64] = {0};
iio_lib_mock_stats iio_lib_mock_stats_data;

static bool mock_is_output(const struct iio_channel *chn) {
  return chn != NULL && chn == iio_lib_mock_tx_channel;
}

static bool mock_should_fail(const char *attr) {
  return mock_fail_attr[0] != '\0' && strcmp(mock_fail_attr, attr) == 0;
}

struct iio_scan_context {
  uint8_t dummy;
};

struct iio_context_info {
  uint8_t dummy;
};

struct iio_context {
  uint8_t dummy;
};

struct iio_device {
  uint8_t dummy;
};

struct iio_channel {
  uint8_t dummy;
};

struct iio_buffer {
  uint8_t dummy;
};

void *iio_buffer_end(const struct iio_buffer *buf) {
  if (iio_lib_mock_rx != NULL) {
    return iio_lib_mock_rx + iio_lib_mock_rx_len;
  }
  return NULL;
}

void *iio_buffer_first(const struct iio_buffer *buf, const struct iio_channel *chn) {
  if (iio_lib_mock_rx != NULL) {
    return iio_lib_mock_rx;
  }
  if (iio_lib_mock_tx != NULL) {
    return iio_lib_mock_tx + iio_lib_mock_tx_len_processed;
  }
  return NULL;
}

ssize_t iio_buffer_push_partial(struct iio_buffer *buf, size_t samples_count) {
  iio_lib_mock_tx_len_processed += (samples_count * 2);
  return samples_count;
}

ssize_t iio_buffer_refill(struct iio_buffer *buf) {
  return 0;
}

struct iio_device *iio_context_find_device(const struct iio_context *ctx, const char *name) {
  if (ad9361_phy == NULL) {
    ad9361_phy = malloc(sizeof(struct iio_device));
  }
  return ad9361_phy;
}

struct iio_channel *iio_device_find_channel(const struct iio_device *dev, const char *name, bool output) {
  if (output) {
    if (iio_lib_mock_tx_channel == NULL) {
      iio_lib_mock_tx_channel = malloc(sizeof(struct iio_channel));
    }
    return iio_lib_mock_tx_channel;
  } else {
    if (iio_lib_mock_rx_channel == NULL) {
      iio_lib_mock_rx_channel = malloc(sizeof(struct iio_channel));
    }
    return iio_lib_mock_rx_channel;
  }
}

void iio_strerror(int err, char *dst, size_t len) {
  //do nothing
}

ssize_t iio_channel_attr_write(const struct iio_channel *chn, const char *attr, const char *src) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  if (strcmp(attr, "gain_control_mode") == 0) {
    strncpy(iio_lib_mock_stats_data.gain_control_mode, src, sizeof(iio_lib_mock_stats_data.gain_control_mode) - 1);
  }
  return 0;
}

int iio_channel_attr_write_longlong(const struct iio_channel *chn, const char *attr, long long val) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  int idx = mock_is_output(chn) ? 1 : 0;
  if (strcmp(attr, "sampling_frequency") == 0) {
    iio_lib_mock_stats_data.sampling_frequency[idx] = val;
    iio_lib_mock_stats_data.sampling_frequency_writes++;
  } else if (strcmp(attr, "rf_bandwidth") == 0) {
    iio_lib_mock_stats_data.rf_bandwidth[idx] = val;
  } else if (strcmp(attr, "frequency") == 0) {
    iio_lib_mock_stats_data.frequency = val;
    iio_lib_mock_stats_data.frequency_writes++;
  } else if (strcmp(attr, "powerdown") == 0) {
    iio_lib_mock_stats_data.powerdown = val;
    iio_lib_mock_stats_data.powerdown_writes++;
  }
  return 0;
}

ssize_t iio_device_attr_write(const struct iio_device *dev, const char *attr, const char *src) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  if (strcmp(attr, "calib_mode") == 0) {
    strncpy(iio_lib_mock_stats_data.calib_mode, src, sizeof(iio_lib_mock_stats_data.calib_mode) - 1);
    iio_lib_mock_stats_data.calib_mode_sampling_frequency_writes = iio_lib_mock_stats_data.sampling_frequency_writes;
  }
  return 0;
}

int iio_device_attr_write_bool(const struct iio_device *dev, const char *attr, bool val) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  if (strcmp(attr, "in_out_voltage_filter_fir_en") == 0) {
    iio_lib_mock_stats_data.fir_enable_calls++;
    iio_lib_mock_stats_data.fir_enabled = val;
  }
  return 0;
}

int iio_channel_attr_write_bool(const struct iio_channel *chn, const char *attr, bool val) {
  return 0;
}

int iio_channel_attr_write_double(const struct iio_channel *chn, const char *attr, double val) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  if (strcmp(attr, "hardwaregain") == 0) {
    int idx = mock_is_output(chn) ? 1 : 0;
    iio_lib_mock_stats_data.hardwaregain[idx] = val;
    iio_lib_mock_stats_data.hardwaregain_writes[idx]++;
  }
  return 0;
}

ssize_t iio_channel_attr_read(const struct iio_channel *chn, const char *attr, char *dst, size_t len) {
  bool output = mock_is_output(chn);
  for (int i = 0; i < MOCK_MAX_ATTRS; i++) {
    if (mock_attrs[i].used && mock_attrs[i].output == output && strcmp(mock_attrs[i].name, attr) == 0) {
      size_t value_len = strlen(mock_attrs[i].value);
      if (value_len > len) {
        value_len = len;
      }
      memcpy(dst, mock_attrs[i].value, value_len);
      return (ssize_t) value_len;
    }
  }
  return -ENOENT;
}

int iio_channel_attr_read_longlong(const struct iio_channel *chn, const char *attr, long long *val) {
  if (strcmp(attr, "sampling_frequency") == 0) {
    *val = mock_current_sample_rate;
    return 0;
  }
  return -ENOENT;
}

ssize_t iio_device_attr_write_raw(const struct iio_device *dev, const char *attr, const void *src, size_t len) {
  if (mock_should_fail(attr)) {
    return -EIO;
  }
  if (strcmp(attr, "filter_fir_config") == 0) {
    iio_lib_mock_stats_data.fir_config_writes++;
    size_t copy_len = len < sizeof(iio_lib_mock_stats_data.fir_config) - 1 ? len : sizeof(iio_lib_mock_stats_data.fir_config) - 1;
    memcpy(iio_lib_mock_stats_data.fir_config, src, copy_len);
    iio_lib_mock_stats_data.fir_config[copy_len] = '\0';
  }
  return 0;
}

struct iio_scan_context *iio_create_scan_context(const char *backend, unsigned int flags) {
  return malloc(sizeof(struct iio_scan_context));
}

ssize_t iio_scan_context_get_info_list(struct iio_scan_context *ctx, struct iio_context_info ***info) {
  *info = malloc(sizeof(struct iio_context_info *) * 1);
  *info[0] = malloc(sizeof(struct iio_context_info));
  return 1;
}

void iio_scan_context_destroy(struct iio_scan_context *ctx) {
  if (ctx == NULL) {
    return;
  }
  free(ctx);
}

const char *iio_context_info_get_uri(const struct iio_context_info *info) {
  return "test";
}

struct iio_context *iio_create_context_from_uri(const char *uri) {
  return malloc(sizeof(struct iio_context));
}

void iio_context_info_list_free(struct iio_context_info **info) {
  free(info[0]);
  free(info);
}

int iio_context_set_timeout(struct iio_context *ctx, unsigned int timeout_ms) {
  return 0;
}

void iio_channel_enable(struct iio_channel *chn) {
  //do nothing
}

struct iio_buffer *iio_device_create_buffer(const struct iio_device *dev, size_t samples_count, bool cyclic) {
  return malloc(sizeof(struct iio_buffer));
}

void iio_buffer_destroy(struct iio_buffer *buf) {
  if (buf == NULL) {
    return;
  }
  free(buf);
}

void iio_channel_disable(struct iio_channel *chn) {
  //do nothing
}

void iio_context_destroy(struct iio_context *ctx) {
  if (ctx == NULL) {
    return;
  }
  free(ctx);
  if (ad9361_phy != NULL) {
    free(ad9361_phy);
    ad9361_phy = NULL;
  }
  if (iio_lib_mock_tx_channel != NULL) {
    free(iio_lib_mock_tx_channel);
    iio_lib_mock_tx_channel = NULL;
  }
  if (iio_lib_mock_rx_channel != NULL) {
    free(iio_lib_mock_rx_channel);
    iio_lib_mock_rx_channel = NULL;
  }
}

static void mock_set_attr(bool output, const char *name, const char *value) {
  mock_attr *free_slot = NULL;
  for (int i = 0; i < MOCK_MAX_ATTRS; i++) {
    if (mock_attrs[i].used && mock_attrs[i].output == output && strcmp(mock_attrs[i].name, name) == 0) {
      free_slot = &mock_attrs[i];
      break;
    }
    if (!mock_attrs[i].used && free_slot == NULL) {
      free_slot = &mock_attrs[i];
    }
  }
  if (free_slot == NULL) {
    return;
  }
  free_slot->used = true;
  free_slot->output = output;
  snprintf(free_slot->name, sizeof(free_slot->name), "%s", name);
  snprintf(free_slot->value, sizeof(free_slot->value), "%s", value);
}

void iio_lib_mock_set_attr(bool output, const char *name, const char *value) {
  mock_set_attr(output, name, value);
}

void iio_lib_mock_set_current_sample_rate(long long sample_rate) {
  mock_current_sample_rate = sample_rate;
}

void iio_lib_mock_fail_write(const char *attr) {
  if (attr == NULL) {
    mock_fail_attr[0] = '\0';
    return;
  }
  snprintf(mock_fail_attr, sizeof(mock_fail_attr), "%s", attr);
}

void iio_lib_mock_reset(void) {
  memset(mock_attrs, 0, sizeof(mock_attrs));
  memset(&iio_lib_mock_stats_data, 0, sizeof(iio_lib_mock_stats_data));
  mock_fail_attr[0] = '\0';
  mock_current_sample_rate = 1000000;
  // typical ad9361 values
  mock_set_attr(false, "hardwaregain_available", "[-1 1 73]");
  mock_set_attr(true, "hardwaregain_available", "[-89.75 0.25 0]");
  mock_set_attr(true, "frequency_available", "[70000000 1 6000000000]");
  mock_set_attr(false, "sampling_frequency_available", "[2083333 1 61440000]");
}

const iio_lib_mock_stats *iio_lib_mock_get_stats(void) {
  return &iio_lib_mock_stats_data;
}

int iio_lib_mock_create(int16_t *expected_rx, size_t expected_rx_len, int16_t *expected_tx, iio_lib **lib) {
  struct iio_lib_t *result = malloc(sizeof(struct iio_lib_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct iio_lib_t){0};
  iio_lib_mock_rx = expected_rx;
  iio_lib_mock_rx_len = expected_rx_len;
  iio_lib_mock_tx = expected_tx;
  iio_lib_mock_tx_len_processed = 0;
  iio_lib_mock_reset();
  result->iio_buffer_destroy = iio_buffer_destroy;
  result->iio_buffer_end = iio_buffer_end;
  result->iio_buffer_first = iio_buffer_first;
  result->iio_buffer_push_partial = iio_buffer_push_partial;
  result->iio_buffer_refill = iio_buffer_refill;
  result->iio_channel_attr_read = iio_channel_attr_read;
  result->iio_channel_attr_read_longlong = iio_channel_attr_read_longlong;
  result->iio_channel_attr_write = iio_channel_attr_write;
  result->iio_channel_attr_write_bool = iio_channel_attr_write_bool;
  result->iio_channel_attr_write_double = iio_channel_attr_write_double;
  result->iio_channel_attr_write_longlong = iio_channel_attr_write_longlong;
  result->iio_channel_disable = iio_channel_disable;
  result->iio_channel_enable = iio_channel_enable;
  result->iio_context_destroy = iio_context_destroy;
  result->iio_context_find_device = iio_context_find_device;
  result->iio_context_info_get_uri = iio_context_info_get_uri;
  result->iio_context_info_list_free = iio_context_info_list_free;
  result->iio_context_set_timeout = iio_context_set_timeout;
  result->iio_create_context_from_uri = iio_create_context_from_uri;
  result->iio_create_scan_context = iio_create_scan_context;
  result->iio_device_attr_write = iio_device_attr_write;
  result->iio_device_attr_write_bool = iio_device_attr_write_bool;
  result->iio_device_attr_write_raw = iio_device_attr_write_raw;
  result->iio_device_create_buffer = iio_device_create_buffer;
  result->iio_device_find_channel = iio_device_find_channel;
  result->iio_scan_context_destroy = iio_scan_context_destroy;
  result->iio_scan_context_get_info_list = iio_scan_context_get_info_list;
  result->iio_strerror = iio_strerror;

  *lib = result;
  return 0;
}

void iio_lib_mock_get_tx(int16_t **output, size_t *output_len) {
  *output = iio_lib_mock_tx;
  *output_len = iio_lib_mock_tx_len_processed;
}

void iio_lib_mock_destroy(iio_lib *lib) {
  if (lib == NULL) {
    return;
  }
  free(lib);
}
