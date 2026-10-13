#include "plutosdr.h"
#include <iio.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include "../log.h"

#define FIR_BUF_SIZE    8192

static char plutosdr_tmpstr[256];
static const size_t plutosdr_tmpstr_len = 256;
static const uint32_t MIN_NO_FIR_FILTER = 2083334; // ((double) 25000000 / 12 + 1)
static const uint32_t MIN_FIR_FILTER_2 = 1041667; // MIN_NO_FIR_FILTER / 2;
static const uint32_t MIN_FIR_FILTER = 520834; // MIN_NO_FIR_FILTER / 4 + 1;
static int16_t fir_128_4[] = {
  -15, -27, -23, -6, 17, 33, 31, 9, -23, -47, -45, -13, 34, 69, 67, 21, -49, -102, -99, -32, 69, 146, 143, 48, -96, -204, -200, -69, 129, 278, 275, 97, -170,
  -372, -371, -135, 222, 494, 497, 187, -288, -654, -665, -258, 376, 875, 902, 363, -500, -1201, -1265, -530, 699, 1748, 1906, 845, -1089, -2922, -3424,
  -1697, 2326, 7714, 12821, 15921, 15921, 12821, 7714, 2326, -1697, -3424, -2922, -1089, 845, 1906, 1748, 699, -530, -1265, -1201, -500, 363, 902, 875,
  376, -258, -665, -654, -288, 187, 497, 494, 222, -135, -371, -372, -170, 97, 275, 278, 129, -69, -200, -204, -96, 48, 143, 146, 69, -32, -99, -102, -49, 21,
  67, 69, 34, -13, -45, -47, -23, 9, 31, 33, 17, -6, -23, -27, -15
};

static int16_t fir_128_2[] = {
  -0, 0, 1, -0, -2, 0, 3, -0, -5, 0, 8, -0, -11, 0, 17, -0, -24, 0, 33, -0, -45, 0, 61, -0, -80, 0, 104, -0, -134, 0, 169, -0,
  -213, 0, 264, -0, -327, 0, 401, -0, -489, 0, 595, -0, -724, 0, 880, -0, -1075, 0, 1323, -0, -1652, 0, 2114, -0, -2819, 0, 4056, -0, -6883, 0, 20837, 32767,
  20837, 0, -6883, -0, 4056, 0, -2819, -0, 2114, 0, -1652, -0, 1323, 0, -1075, -0, 880, 0, -724, -0, 595, 0, -489, -0, 401, 0, -327, -0, 264, 0, -213, -0,
  169, 0, -134, -0, 104, 0, -80, -0, 61, 0, -45, -0, 33, 0, -24, -0, 17, 0, -11, -0, 8, 0, -5, -0, 3, 0, -2, -0, 1, 0, -0, 0
};

typedef struct {
  double min;
  double step;
  double max;
} plutosdr_range;

struct plutosdr_t {
  struct iio_context *iio_ctx;
  struct iio_device *tx;
  struct iio_channel *tx0_chn;
  struct iio_channel *tx0_lo;
  struct iio_channel *tx0_i;
  struct iio_channel *tx0_q;
  struct iio_buffer *tx_buffer;

  struct iio_device *rx;
  struct iio_channel *rx0_chn;
  struct iio_channel *rx0_lo;
  struct iio_channel *rx0_i;
  struct iio_channel *rx0_q;
  struct iio_buffer *rx_buffer;

  plutosdr_range gain;
  plutosdr_range power;
  plutosdr_range frequency;
  plutosdr_range sample_rate;

  long long rx_current_sample_rate;
  long long tx_current_sample_rate;

  char *uri;

  float complex *output;
  size_t sdr_buffer_len;

  iio_lib *lib;
  iio_gain_mode gain_control_mode;
};

enum iio_direction {
  RX, TX
};

int plutosdr_process_tx(float complex *input, size_t input_len, void *plugin) {
  plutosdr *iio = (plutosdr *) plugin;
  if (iio->tx_buffer == NULL) {
    LOG_ERROR("tx was not initialized");
    return -1;
  }

  size_t processed = 0;
  size_t left = input_len;
  while (left > 0) {
    char *p_start = (char *) iio->lib->iio_buffer_first(iio->tx_buffer, iio->tx0_i);
    size_t batch;
    if (left > iio->sdr_buffer_len / 2) {
      batch = iio->sdr_buffer_len / 2;
    } else {
      batch = left;
    }

    // put [-1;1] into 16bit interval [-32768;32768]
    // pluto has 16bit DAC for TX
    int16_t *tx_output = (int16_t *) p_start;
    const float *tx_input = (const float *) (input + processed);
    for (size_t i = 0; i < batch * 2; i++) {
      float r = tx_input[i] * 32768;
      if (r > SHRT_MAX) {
        r = SHRT_MAX;
      } else if (r < SHRT_MIN) {
        r = SHRT_MIN;
      }
      tx_output[i] = (int16_t) rintf(r);
    }

    // always use push_partial because normal push won't reset buffer to full length
    // https://github.com/analogdevicesinc/libiio/blob/e65a97863c3481f30c6ea8642bda86125a7ee39d/buffer.c#L154
    ssize_t nbytes_tx = iio->lib->iio_buffer_push_partial(iio->tx_buffer, batch);
    if (nbytes_tx < 0) {
      return -1;
    }

    left -= batch;
    processed += batch;
  }

  return 0;
}

int plutosdr_process_rx(float complex **output, size_t *output_len, void *plugin) {
  plutosdr *iio = (plutosdr *) plugin;
  if (iio->rx_buffer == NULL) {
    LOG_ERROR("rx was not initialized");
    *output = NULL;
    *output_len = 0;
    return -1;
  }
  ssize_t ret = iio->lib->iio_buffer_refill(iio->rx_buffer);
  if (ret < 0) {
    *output = NULL;
    *output_len = 0;
    return -1;
  }

  char *p_end = iio->lib->iio_buffer_end(iio->rx_buffer);
  char *p_start = (char *) iio->lib->iio_buffer_first(iio->rx_buffer, iio->rx0_i);
  size_t num_points = (p_end - p_start) / sizeof(int16_t);
  if (num_points / 2 > iio->sdr_buffer_len) {
    LOG_ERROR("input buffer %zu is more than max: %zu", num_points / 2, iio->sdr_buffer_len);
    *output = NULL;
    *output_len = 0;
    return -1;
  }
  // ADC is 12bit, thus 2^12 = 2048
  float *rx_output = (float *) iio->output;
  const int16_t *rx_input = (const int16_t *) p_start;
  for (size_t i = 0; i < num_points; i++) {
    rx_output[i] = ((float) rx_input[i]) / 2048.0F;
  }
  *output = iio->output;
  *output_len = num_points / 2;
  return 0;
}

// pluto can generate tones at certain frequencies, phases and amplitudes.
// they are controlled by the TX1_I_F1/TX1_Q_F1/TX1_I_F2/TX1_Q_F2
// these parameters can be saved into pluto's persistent memory and accidentally loaded
// when the data transmitted by sdr-modem, it can be affected by the DDS tones, thus DDS tones must be disabled
static int plutosdr_disable_dds(plutosdr *pluto) {
  // normally disabling just TX1_I_F1 would disable all other DDS channels
  // but I disable all just in case
  struct iio_channel *channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_I_F1", true);
  if (channel == NULL) {
    return -1;
  }
  int code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
  if (code < 0) {
    return code;
  }
  channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_Q_F1", true);
  if (channel == NULL) {
    return -1;
  }
  code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
  if (code < 0) {
    return code;
  }
  channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_Q_F2", true);
  if (channel == NULL) {
    return -1;
  }
  code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
  if (code < 0) {
    return code;
  }
  channel = pluto->lib->iio_device_find_channel(pluto->tx, "TX1_I_F2", true);
  if (channel == NULL) {
    return -1;
  }
  code = pluto->lib->iio_channel_attr_write_bool(channel, "raw", 0);
  if (code < 0) {
    return code;
  }
  return 0;
}

static struct iio_channel *plutosdr_find_phy_channel(struct iio_context *ctx, enum iio_direction d, plutosdr *iio) {
  switch (d) {
    case RX:
      return iio->lib->iio_device_find_channel(iio->lib->iio_context_find_device(ctx, "ad9361-phy"), "voltage0", false);
    case TX:
      return iio->lib->iio_device_find_channel(iio->lib->iio_context_find_device(ctx, "ad9361-phy"), "voltage0", true);
    default:
      return NULL;
  }
}

static int plutosdr_error_check(int v, const char *what, plutosdr *iio) {
  if (v < 0) {
    iio->lib->iio_strerror(-v, plutosdr_tmpstr, plutosdr_tmpstr_len);
    LOG_ERROR("unable to write value for \"%s\": %s", what, plutosdr_tmpstr);
    return v;
  }
  return 0;
}

static int plutosdr_write_str(struct iio_channel *chn, const char *what, const char *str, plutosdr *iio) {
  return plutosdr_error_check(iio->lib->iio_channel_attr_write(chn, what, str), what, iio);
}

static int plutosdr_write_lli(struct iio_channel *chn, const char *what, long long val, plutosdr *iio) {
  return plutosdr_error_check(iio->lib->iio_channel_attr_write_longlong(chn, what, val), what, iio);
}

static int plutosdr_read_range(struct iio_channel *chn, const char *name, plutosdr_range *range, plutosdr *iio) {
  char buf[128];
  ssize_t len = iio->lib->iio_channel_attr_read(chn, name, buf, sizeof(buf) - 1);
  if (len < 0) {
    iio->lib->iio_strerror((int) -len, plutosdr_tmpstr, plutosdr_tmpstr_len);
    LOG_ERROR("unable to read \"%s\": %s", name, plutosdr_tmpstr);
    return (int) len;
  }
  buf[len] = '\0';

  const char *p = buf;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '[') {
    LOG_ERROR("unexpected format of \"%s\": %s", name, buf);
    return -EINVAL;
  }
  p++;
  double values[3];
  for (int i = 0; i < 3; i++) {
    char *end = NULL;
    values[i] = strtod(p, &end);
    if (end == p) {
      LOG_ERROR("unexpected format of \"%s\": %s", name, buf);
      return -EINVAL;
    }
    p = end;
  }
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != ']') {
    LOG_ERROR("unexpected format of \"%s\": %s", name, buf);
    return -EINVAL;
  }
  range->min = values[0];
  range->step = values[1];
  range->max = values[2];
  return 0;
}

int plutosdr_enable_fir_filter(struct iio_device *dev, bool enable, plutosdr *pluto) {
  int ret = pluto->lib->iio_device_attr_write_bool(dev, "in_out_voltage_filter_fir_en", enable);
  if (ret < 0) {
    ret = pluto->lib->iio_channel_attr_write_bool(pluto->lib->iio_device_find_channel(dev, "out", false), "voltage_filter_fir_en", enable);
  }
  return ret;
}

static int plutosdr_set_sample_rate(uint64_t sample_rate, struct iio_channel *chn, plutosdr *iio) {
  int code = plutosdr_write_lli(chn, "rf_bandwidth", (long long) sample_rate, iio);
  if (code != 0) {
    return code;
  }
  return plutosdr_write_lli(chn, "sampling_frequency", (long long) sample_rate, iio);
}

// static int plutosdr_select_fir_filter_config(uint64_t sample_rate, int *decimation, int16_t **fir_filter_taps) {
//   if (sample_rate == 0) {
//     *decimation = 0;
//     *fir_filter_taps = NULL;
//     return 0;
//   }
//
//   if (sample_rate < MIN_FIR_FILTER) {
//     LOG_ERROR("sampling freq is too low: %" PRIu64, sample_rate);
//     return -1;
//   } else if (sample_rate < MIN_FIR_FILTER_2) {
//     *decimation = 4;
//     *fir_filter_taps = fir_128_4;
//   } else if (sample_rate < MIN_NO_FIR_FILTER) {
//     *decimation = 2;
//     *fir_filter_taps = fir_128_2;
//   }
//   return 0;
// }

// static int plutosdr_setup_fir_filter(struct iio_context *ctx, uint64_t rx_sampling_frequency, uint64_t tx_sampling_frequency, plutosdr *pluto) {
//   int rx_decimation = 0;
//   int16_t *rx_fir_filter_taps = NULL;
//   int code = plutosdr_select_fir_filter_config(rx_sampling_frequency, &rx_decimation, &rx_fir_filter_taps);
//   if (code < 0) {
//     return code;
//   }
//   int tx_decimation = 0;
//   int16_t *tx_fir_filter_taps = NULL;
//   code = plutosdr_select_fir_filter_config(tx_sampling_frequency, &tx_decimation, &tx_fir_filter_taps);
//   if (code < 0) {
//     return code;
//   }
//
//   struct iio_device *phy_device = pluto->lib->iio_context_find_device(ctx, "ad9361-phy");
//
//   // filter is not needed
//   if (rx_fir_filter_taps == NULL && tx_fir_filter_taps == NULL) {
//     //setup sampling freq a bit more to disable fir filter without error
//     struct iio_channel *chn = plutosdr_find_phy_channel(ctx, TX, pluto);
//     if (chn == NULL) {
//       return -1;
//     }
//     code = plutosdr_write_lli(chn, "sampling_frequency", MIN_NO_FIR_FILTER, pluto);
//     if (code < 0) {
//       return code;
//     }
//     chn = plutosdr_find_phy_channel(ctx, RX, pluto);
//     if (chn == NULL) {
//       return -1;
//     }
//     code = plutosdr_write_lli(chn, "sampling_frequency", MIN_NO_FIR_FILTER, pluto);
//     if (code < 0) {
//       return code;
//     }
//     // filter might be configured prior to execution. disable it to support higher rates
//     return plutosdr_error_check(plutosdr_enable_fir_filter(phy_device, false, pluto), "in_out_voltage_filter_fir_en", pluto);
//   }
//
//   // just to simplify the code below a bit
//   if (rx_fir_filter_taps != NULL && tx_fir_filter_taps == NULL) {
//     tx_fir_filter_taps = rx_fir_filter_taps;
//     tx_decimation = rx_decimation;
//   }
//   if (rx_fir_filter_taps == NULL && tx_fir_filter_taps != NULL) {
//     rx_fir_filter_taps = tx_fir_filter_taps;
//     rx_decimation = tx_decimation;
//   }
//
//   char *buf = malloc(sizeof(char) * FIR_BUF_SIZE);
//   if (buf == NULL) {
//     return -ENOMEM;
//   }
//   int len = 0;
//   if (rx_decimation > 0) {
//     len += snprintf(buf + len, FIR_BUF_SIZE - len, "RX 3 GAIN -6 DEC %d\n", rx_decimation);
//   }
//   if (tx_decimation > 0) {
//     len += snprintf(buf + len, FIR_BUF_SIZE - len, "TX 3 GAIN 0 INT %d\n", tx_decimation);
//   }
//   for (int i = 0; i < 128; i++) {
//     len += snprintf(buf + len, FIR_BUF_SIZE - len, "%d,%d\n", tx_fir_filter_taps[i], rx_fir_filter_taps[i]);
//   }
//   len += snprintf(buf + len, FIR_BUF_SIZE - len, "\n");
//
//   code = plutosdr_error_check(pluto->lib->iio_device_attr_write_raw(phy_device, "filter_fir_config", buf, len), "filter_fir_config", pluto);
//   free(buf);
//   if (code < 0) {
//     return code;
//   }
//
//   code = plutosdr_error_check(plutosdr_enable_fir_filter(phy_device, true, pluto), "in_out_voltage_filter_fir_en", pluto);
//   if (code < 0) {
//     return code;
//   }
//
//   return 0;
// }

static char *plutosdr_find_default_uri(const iio_lib *lib) {
  struct iio_scan_context *scan_ctx = lib->iio_create_scan_context(NULL, 0);
  if (scan_ctx == NULL) {
    LOG_ERROR("unable to scan: %s", strerror(errno));
    return NULL;
  }
  struct iio_context_info **info = NULL;
  ssize_t code = lib->iio_scan_context_get_info_list(scan_ctx, &info);
  if (code < 0) {
    LOG_ERROR("unable to scan contexts: %zd", code);
    lib->iio_scan_context_destroy(scan_ctx);
    return NULL;
  }
  if (code == 0) {
    LOG_ERROR("no sdr contexts found");
    lib->iio_scan_context_destroy(scan_ctx);
    return NULL;
  }
  char *result = strdup(lib->iio_context_info_get_uri(info[0]));
  lib->iio_context_info_list_free(info);
  lib->iio_scan_context_destroy(scan_ctx);
  return result;
}

// rx and tx share the same baseband clock, so their sampling rates can differ only by an integer ratio of 1, 2 or 4
// static int plutosdr_validate_sampling_frequencies(uint64_t rx_sampling_frequency, uint64_t tx_sampling_frequency) {
//   if (rx_sampling_frequency == 0 || tx_sampling_frequency == 0) {
//     return 0;
//   }
//   uint64_t max = rx_sampling_frequency > tx_sampling_frequency ? rx_sampling_frequency : tx_sampling_frequency;
//   uint64_t min = rx_sampling_frequency > tx_sampling_frequency ? tx_sampling_frequency : rx_sampling_frequency;
//   if (max % min != 0 || (max / min != 1 && max / min != 2 && max / min != 4)) {
//     LOG_ERROR("incompatible sampling frequencies: rx %" PRIu64 ", tx %" PRIu64 ". they must be equal or differ by a factor of 2 or 4", rx_sampling_frequency, tx_sampling_frequency);
//     return -1;
//   }
//   return 0;
// }

static const char *plutosdr_convert_gain_mode(iio_gain_mode mode) {
  switch (mode) {
    case IIO_GAIN_MODE_MANUAL:
      return "manual";
    case IIO_GAIN_MODE_FAST_ATTACK:
      return "fast_attack";
    case IIO_GAIN_MODE_SLOW_ATTACK:
      return "slow_attack";
    case IIO_GAIN_MODE_HYBRID:
      return "hybrid";
    default:
      LOG_ERROR("unknown gain mode: %d", mode);
      return NULL;
  }
}

void plutosdr_start_rx(void *plugin) {
  plutosdr *pluto = (plutosdr *) plugin;
  pluto->lib->iio_channel_enable(pluto->rx0_i);
  pluto->lib->iio_channel_enable(pluto->rx0_q);
  if (pluto->rx_buffer != NULL) {
    pluto->lib->iio_buffer_destroy(pluto->rx_buffer);
  }
  pluto->rx_buffer = pluto->lib->iio_device_create_buffer(pluto->rx, pluto->sdr_buffer_len, false);
  if (pluto->rx_buffer == NULL) {
    LOG_ERROR("unable to create rx buffer: %s", strerror(errno));
  }
}

void plutosdr_stop_rx(void *plugin) {
  plutosdr *pluto = (plutosdr *) plugin;
  pluto->lib->iio_channel_disable(pluto->rx0_i);
  pluto->lib->iio_channel_disable(pluto->rx0_q);
  if (pluto->rx_buffer != NULL) {
    pluto->lib->iio_buffer_destroy(pluto->rx_buffer);
    pluto->rx_buffer = NULL;
  }
}

int plutosdr_create(const plutosdr_settings *settings, uint32_t sdr_buffer_len, iio_lib *lib, sdr_device **output) {
  struct plutosdr_t *pluto = malloc(sizeof(struct plutosdr_t));
  if (pluto == NULL) {
    return -ENOMEM;
  }
  *pluto = (struct plutosdr_t){0};
  pluto->lib = lib;
  pluto->sdr_buffer_len = sdr_buffer_len;

  if (settings->uri == NULL) {
    pluto->uri = plutosdr_find_default_uri(lib);
  } else {
    pluto->uri = strdup(settings->uri);
  }
  if (pluto->uri == NULL) {
    LOG_ERROR("unable to find plutosdr context. specify plutosdr_uri explicitly");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->iio_ctx = lib->iio_create_context_from_uri(pluto->uri);
  if (pluto->iio_ctx == NULL) {
    LOG_ERROR("unable to initialize context from: %s", pluto->uri);
    plutosdr_destroy(pluto);
    return -1;
  }

  int code = pluto->lib->iio_context_set_timeout(pluto->iio_ctx, settings->timeout_ms);
  if (code < 0) {
    LOG_ERROR("unable to setup timeout: %d", code);
    plutosdr_destroy(pluto);
    return -1;
  }

  const char *gain_control_mode_str = plutosdr_convert_gain_mode(settings->gain_control_mode);
  if (gain_control_mode_str == NULL) {
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->rx = pluto->lib->iio_context_find_device(pluto->iio_ctx, "cf-ad9361-lpc");
  if (pluto->rx == NULL) {
    LOG_ERROR("unable to find rx result");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->rx0_chn = plutosdr_find_phy_channel(pluto->iio_ctx, RX, pluto);
  if (pluto->rx0_chn == NULL) {
    LOG_ERROR("can't find RX channel: voltage0");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->rx0_lo = pluto->lib->iio_device_find_channel(pluto->lib->iio_context_find_device(pluto->iio_ctx, "ad9361-phy"), "altvoltage0", true);
  if (pluto->rx0_lo == NULL) {
    LOG_ERROR("can't find RX lo");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->rx0_i = pluto->lib->iio_device_find_channel(pluto->rx, "voltage0", false);
  if (pluto->rx0_i == NULL) {
    LOG_ERROR("unable to find rx I channel");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->rx0_q = pluto->lib->iio_device_find_channel(pluto->rx, "voltage1", false);
  if (pluto->rx0_q == NULL) {
    LOG_ERROR("unable to find rx Q channel");
    plutosdr_destroy(pluto);
    return -1;
  }
  code = plutosdr_write_str(pluto->rx0_chn, "gain_control_mode", gain_control_mode_str, pluto);
  if (code < 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  pluto->gain_control_mode = settings->gain_control_mode;
  pluto->output = malloc(sizeof(float complex) * pluto->sdr_buffer_len);
  if (pluto->output == NULL) {
    plutosdr_destroy(pluto);
    return -ENOMEM;
  }
  code = plutosdr_read_range(pluto->rx0_chn, "hardwaregain_available", &pluto->gain, pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  // both rx and tx have the same range of supported frequencies and sample rates
  code = plutosdr_read_range(pluto->rx0_lo, "frequency_available", &pluto->frequency, pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  code = plutosdr_read_range(pluto->rx0_chn, "sampling_frequency_available", &pluto->sample_rate, pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  code = plutosdr_error_check(pluto->lib->iio_channel_attr_read_longlong(pluto->rx0_chn, "sampling_frequency", &pluto->rx_current_sample_rate), "sampling_frequency", pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  if (settings->sample_rate != 0 && settings->sample_rate != pluto->rx_current_sample_rate) {
    code = plutosdr_set_sample_rate(settings->sample_rate, pluto->rx0_chn, pluto);
    if (code == 0) {
      pluto->rx_current_sample_rate = settings->sample_rate;
    }
  }

  // TX chain //////////////////////////////
  pluto->tx = pluto->lib->iio_context_find_device(pluto->iio_ctx, "cf-ad9361-dds-core-lpc");
  if (pluto->tx == NULL) {
    LOG_ERROR("unable to find tx device");
    plutosdr_destroy(pluto);
    return -1;
  }
  code = plutosdr_disable_dds(pluto);
  if (code < 0) {
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->tx0_chn = plutosdr_find_phy_channel(pluto->iio_ctx, TX, pluto);
  if (pluto->tx0_chn == NULL) {
    LOG_ERROR("can't find TX channel: voltage0");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->tx0_lo = pluto->lib->iio_device_find_channel(pluto->lib->iio_context_find_device(pluto->iio_ctx, "ad9361-phy"), "altvoltage1", true);
  if (pluto->tx0_lo == NULL) {
    LOG_ERROR("can't find TX lo");
    plutosdr_destroy(pluto);
    return -1;
  }
  if (settings->tx_powerdown) {
    // completely disabling TX when doing RX only will significantly improve sensitivity
    // details: https://wiki.analog.com/university/tools/pluto/hacking/listening_to_yourself
    plutosdr_write_lli(pluto->tx0_lo, "powerdown", 1, pluto);
  }
  pluto->tx0_i = pluto->lib->iio_device_find_channel(pluto->tx, "voltage0", true);
  if (pluto->tx0_i == NULL) {
    LOG_ERROR("unable to find tx I channel");
    plutosdr_destroy(pluto);
    return -1;
  }
  pluto->tx0_q = pluto->lib->iio_device_find_channel(pluto->tx, "voltage1", true);
  if (pluto->tx0_q == NULL) {
    LOG_ERROR("unable to find tx Q channel");
    plutosdr_destroy(pluto);
    return -1;
  }
  code = plutosdr_read_range(pluto->tx0_chn, "hardwaregain_available", &pluto->power, pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  code = plutosdr_error_check(pluto->lib->iio_channel_attr_read_longlong(pluto->tx0_chn, "sampling_frequency", &pluto->tx_current_sample_rate), "sampling_frequency", pluto);
  if (code != 0) {
    plutosdr_destroy(pluto);
    return code;
  }
  if (settings->sample_rate != 0 && settings->sample_rate != pluto->tx_current_sample_rate) {
    code = plutosdr_set_sample_rate(settings->sample_rate, pluto->tx0_chn, pluto);
    if (code == 0) {
      pluto->tx_current_sample_rate = settings->sample_rate;
    }
  }

  struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
  if (result == NULL) {
    plutosdr_destroy(pluto);
    return -ENOMEM;
  }
  result->plugin = pluto;
  result->sdr_process_rx = plutosdr_process_rx;
  result->sdr_process_tx = plutosdr_process_tx;
  result->destroy = plutosdr_destroy;
  result->set_rx_parameters = plutosdr_set_rx_parameters;
  result->set_tx_parameters = plutosdr_set_tx_parameters;
  result->start_rx = plutosdr_start_rx;
  result->stop_rx = plutosdr_stop_rx;

  LOG_INFO("plutosdr: uri=%s gain_control_mode=%d tx_powerdown=%s timeout_ms=%u frequency=%.0f~%.0f sample_rate=%.0f~%.0f gain=%.0f~%.0f power=%.2f~%.2f",
           pluto->uri, settings->gain_control_mode, settings->tx_powerdown ? "true" : "false", settings->timeout_ms,
           pluto->frequency.min, pluto->frequency.max,
           pluto->sample_rate.min, pluto->sample_rate.max,
           pluto->gain.min, pluto->gain.max,
           pluto->power.min, pluto->power.max);
  *output = result;
  return 0;
}

int plutosdr_set_rx_parameters(sdr_channel_config *config, void *plugin) {
  struct plutosdr_t *pluto = (struct plutosdr_t *) plugin;
  if (config->frequency < pluto->frequency.min || config->frequency > pluto->frequency.max) {
    LOG_ERROR("frequency is outside of supported range: %"PRIu64, config->frequency);
    return -1;
  }
  if (config->sample_rate < pluto->sample_rate.min || config->sample_rate > pluto->sample_rate.max) {
    LOG_ERROR("sample_rate is outside of supported range: %"PRIu64, config->sample_rate);
    return -1;
  }
  if (config->gain < pluto->gain.min || config->gain > pluto->gain.max) {
    LOG_ERROR("gain is outside of supported range: %.0f", config->gain);
    return -1;
  }
  int code = plutosdr_write_lli(pluto->rx0_lo, "frequency", (long long) config->frequency, pluto);
  if (code != 0) {
    return code;
  }
  if (pluto->rx_current_sample_rate != config->sample_rate) {
    code = plutosdr_set_sample_rate(config->sample_rate, pluto->rx0_chn, pluto);
    if (code != 0) {
      return code;
    } else {
      pluto->rx_current_sample_rate = config->sample_rate;
    }
  }
  if (pluto->gain_control_mode == IIO_GAIN_MODE_MANUAL) {
    code = plutosdr_error_check(pluto->lib->iio_channel_attr_write_double(pluto->rx0_chn, "hardwaregain", config->gain), "hardwaregain", pluto);
    if (code != 0) {
      return code;
    }
  }
  LOG_INFO("plutosdr rx: frequency=%" PRIu64 " sample_rate=%" PRIu64 " gain=%.1f", config->frequency, config->sample_rate, config->gain);
  return 0;
}

int plutosdr_set_tx_parameters(sdr_channel_config *config, void *plugin) {
  struct plutosdr_t *pluto = (struct plutosdr_t *) plugin;
  if (config->frequency < pluto->frequency.min || config->frequency > pluto->frequency.max) {
    LOG_ERROR("frequency is outside of supported range: %"PRIu64, config->frequency);
    return -1;
  }
  if (config->sample_rate < pluto->sample_rate.min || config->sample_rate > pluto->sample_rate.max) {
    LOG_ERROR("sample_rate is outside of supported range: %"PRIu64, config->sample_rate);
    return -1;
  }
  if (config->gain < pluto->power.min || config->gain > pluto->power.max) {
    LOG_ERROR("power is outside of supported range: %.0f", config->gain);
    return -1;
  }
  int code = plutosdr_write_lli(pluto->tx0_lo, "frequency", (long long) config->frequency, pluto);
  if (code != 0) {
    return code;
  }
  if (pluto->tx_current_sample_rate != config->sample_rate) {
    code = plutosdr_set_sample_rate(config->sample_rate, pluto->tx0_chn, pluto);
    if (code != 0) {
      return code;
    } else {
      pluto->tx_current_sample_rate = (long long) config->sample_rate;
    }
  }
  code = plutosdr_error_check(pluto->lib->iio_channel_attr_write_double(pluto->tx0_chn, "hardwaregain", config->gain), "hardwaregain", pluto);
  if (code != 0) {
    return code;
  }

  pluto->lib->iio_channel_enable(pluto->tx0_i);
  pluto->lib->iio_channel_enable(pluto->tx0_q);
  if (pluto->tx_buffer != NULL) {
    pluto->lib->iio_buffer_destroy(pluto->tx_buffer);
    pluto->tx_buffer = NULL;
  }
  pluto->tx_buffer = pluto->lib->iio_device_create_buffer(pluto->tx, pluto->sdr_buffer_len, false);
  if (pluto->tx_buffer == NULL) {
    LOG_ERROR("unable to create tx buffer: %s", strerror(errno));
    return -1;
  }
  LOG_INFO("plutosdr tx: frequency=%" PRIu64 " sample_rate=%" PRIu64 " power=%.1f", config->frequency, config->sample_rate, config->gain);
  return 0;
}

void plutosdr_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  plutosdr *pluto = (plutosdr *) plugin;
  if (pluto->tx_buffer != NULL) {
    pluto->lib->iio_buffer_destroy(pluto->tx_buffer);
  }
  if (pluto->tx0_i != NULL) {
    pluto->lib->iio_channel_disable(pluto->tx0_i);
  }
  if (pluto->tx0_q != NULL) {
    pluto->lib->iio_channel_disable(pluto->tx0_q);
  }
  if (pluto->rx_buffer != NULL) {
    pluto->lib->iio_buffer_destroy(pluto->rx_buffer);
  }
  if (pluto->rx0_i != NULL) {
    pluto->lib->iio_channel_disable(pluto->rx0_i);
  }
  if (pluto->rx0_q != NULL) {
    pluto->lib->iio_channel_disable(pluto->rx0_q);
  }
  if (pluto->output != NULL) {
    free(pluto->output);
  }
  if (pluto->iio_ctx != NULL) {
    pluto->lib->iio_context_destroy(pluto->iio_ctx);
  }
  if (pluto->uri != NULL) {
    free(pluto->uri);
  }
  free(pluto);
}
