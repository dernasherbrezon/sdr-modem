#include "airspy.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "../log.h"

// supported by r820t tuner
#define AIRSPY_MIN_FREQUENCY 24000000ULL
#define AIRSPY_MAX_FREQUENCY 1750000000ULL
#define AIRSPY_DEFAULT_TIMEOUT_MS 10000
// the device delivers large blocks (64k samples), thus the ring must hold several of them
#define AIRSPY_MIN_RING_SAMPLES 262144
#define AIRSPY_RING_BUFFERS 16

#define ERROR_CHECK(x, dev)           \
    do {                           \
      int __err_rc = (x);          \
      if (__err_rc != 0) { \
        LOG_ERROR("airspy failure: %s (%d)", (dev)->lib->airspy_error_name(__err_rc), __err_rc); \
        return __err_rc;           \
      }                            \
    } while (0)

struct airspy_t {
  airspy_lib *lib;
  struct airspy_device *device;

  airspy_gain_mode gain_mode;
  uint8_t lna_gain;
  uint8_t mixer_gain;
  uint8_t vga_gain;
  unsigned int timeout_ms;

  uint32_t *sample_rates;
  size_t sample_rates_len;
  // 0 - unknown
  uint64_t current_sample_rate;

  size_t sdr_buffer_len;
  float complex *output;

  // ring buffer filled by the libairspy streaming thread and drained by airspy_process_rx
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool sync_initialized;
  float complex *ring;
  size_t ring_len;
  size_t ring_head;
  size_t ring_count;
  uint64_t dropped_samples;
  bool rx_enabled;
};

// static int airspy_check(int code, const char *what, airspy *dev) {
//   if (code != AIRSPY_LIB_SUCCESS) {
//     LOG_ERROR("unable to %s: %s (%d)", what, dev->lib->airspy_error_name(code), code);
//     return -1;
//   }
//   return 0;
// }

static const char *airspy_gain_mode_name(airspy_gain_mode mode) {
  switch (mode) {
    case AIRSPY_GAIN_MODE_AUTO:
      return "auto";
    case AIRSPY_GAIN_MODE_SENSITIVITY:
      return "sensitivity";
    case AIRSPY_GAIN_MODE_LINEARITY:
      return "linearity";
    case AIRSPY_GAIN_MODE_MANUAL:
      return "manual";
    default:
      return NULL;
  }
}

// runs in the libairspy streaming thread
static int airspy_rx_callback(airspy_lib_transfer *transfer) {
  airspy *dev = (airspy *) transfer->ctx;
  pthread_mutex_lock(&dev->mutex);
  if (!dev->rx_enabled) {
    pthread_mutex_unlock(&dev->mutex);
    return 0;
  }
  size_t samples = transfer->sample_count > 0 ? (size_t) transfer->sample_count : 0;
  dev->dropped_samples += transfer->dropped_samples;
  if (transfer->sample_type != AIRSPY_LIB_SAMPLE_FLOAT32_IQ || samples > dev->ring_len - dev->ring_count) {
    // never write partial blocks: it is better to lose the whole block than to glue unrelated parts together
    dev->dropped_samples += samples;
    pthread_mutex_unlock(&dev->mutex);
    return 0;
  }
  const float complex *input = (const float complex *) transfer->samples;
  size_t tail = (dev->ring_head + dev->ring_count) % dev->ring_len;
  size_t first = dev->ring_len - tail;
  if (first > samples) {
    first = samples;
  }
  memcpy(dev->ring + tail, input, first * sizeof(float complex));
  memcpy(dev->ring, input + first, (samples - first) * sizeof(float complex));
  dev->ring_count += samples;
  pthread_cond_signal(&dev->cond);
  pthread_mutex_unlock(&dev->mutex);
  return 0;
}

static int airspy_process_rx(float complex **output, size_t *output_len, void *plugin) {
  airspy *dev = (airspy *) plugin;
  *output = NULL;
  *output_len = 0;

  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_sec += dev->timeout_ms / 1000;
  deadline.tv_nsec += (long) (dev->timeout_ms % 1000) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec++;
    deadline.tv_nsec -= 1000000000L;
  }

  pthread_mutex_lock(&dev->mutex);
  if (!dev->rx_enabled) {
    pthread_mutex_unlock(&dev->mutex);
    LOG_ERROR("rx was not started");
    return -1;
  }
  while (dev->rx_enabled && dev->ring_count < dev->sdr_buffer_len) {
    if (pthread_cond_timedwait(&dev->cond, &dev->mutex, &deadline) == ETIMEDOUT) {
      break;
    }
  }
  if (!dev->rx_enabled) {
    // stopped while waiting
    pthread_mutex_unlock(&dev->mutex);
    return -1;
  }
  if (dev->ring_count < dev->sdr_buffer_len) {
    pthread_mutex_unlock(&dev->mutex);
    if (dev->lib->airspy_is_streaming(dev->device) != AIRSPY_LIB_TRUE) {
      LOG_ERROR("airspy stopped streaming");
      return -1;
    }
    // any code below -1 is treated by the caller as a recoverable timeout
    return -ETIMEDOUT;
  }

  size_t first = dev->ring_len - dev->ring_head;
  if (first > dev->sdr_buffer_len) {
    first = dev->sdr_buffer_len;
  }
  memcpy(dev->output, dev->ring + dev->ring_head, first * sizeof(float complex));
  memcpy(dev->output + first, dev->ring, (dev->sdr_buffer_len - first) * sizeof(float complex));
  dev->ring_head = (dev->ring_head + dev->sdr_buffer_len) % dev->ring_len;
  dev->ring_count -= dev->sdr_buffer_len;
  uint64_t dropped = dev->dropped_samples;
  dev->dropped_samples = 0;
  pthread_mutex_unlock(&dev->mutex);

  if (dropped > 0) {
    LOG_WARN("airspy dropped %" PRIu64 " samples", dropped);
  }
  *output = dev->output;
  *output_len = dev->sdr_buffer_len;
  return 0;
}

static int airspy_set_agc(airspy *dev, uint8_t enabled) {
  ERROR_CHECK(dev->lib->airspy_set_lna_agc(dev->device, enabled), dev);
  ERROR_CHECK(dev->lib->airspy_set_mixer_agc(dev->device, enabled), dev);
  return 0;
}

static int airspy_apply_gain(float gain, airspy *dev) {
  int code;
  switch (dev->gain_mode) {
    case AIRSPY_GAIN_MODE_AUTO:
      code = airspy_set_agc(dev, 1);
      if (code != 0) {
        return code;
      }
      ERROR_CHECK(dev->lib->airspy_set_vga_gain(dev->device, dev->vga_gain), dev);
      return 0;
    case AIRSPY_GAIN_MODE_SENSITIVITY:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      ERROR_CHECK(dev->lib->airspy_set_sensitivity_gain(dev->device, (uint8_t) lroundf(gain)), dev);
      return 0;
    case AIRSPY_GAIN_MODE_LINEARITY:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      ERROR_CHECK(dev->lib->airspy_set_linearity_gain(dev->device, (uint8_t) lroundf(gain)), dev);
      return 0;
    case AIRSPY_GAIN_MODE_MANUAL:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      ERROR_CHECK(dev->lib->airspy_set_lna_gain(dev->device, dev->lna_gain), dev);
      ERROR_CHECK(dev->lib->airspy_set_mixer_gain(dev->device, dev->mixer_gain), dev);
      ERROR_CHECK(dev->lib->airspy_set_vga_gain(dev->device, dev->vga_gain), dev);
      return 0;
    default:
      return -1;
  }
}

static bool airspy_is_sample_rate_supported(uint64_t sample_rate, const airspy *dev) {
  for (size_t i = 0; i < dev->sample_rates_len; i++) {
    if (dev->sample_rates[i] == sample_rate) {
      return true;
    }
  }
  return false;
}

static int airspy_set_sample_rate(uint64_t sample_rate, airspy *dev) {
  if (!airspy_is_sample_rate_supported(sample_rate, dev)) {
    LOG_ERROR("sample_rate is not supported: %" PRIu64, sample_rate);
    return -1;
  }
  ERROR_CHECK(dev->lib->airspy_set_samplerate(dev->device, (uint32_t) sample_rate), dev);
  dev->current_sample_rate = sample_rate;
  return 0;
}

static void airspy_start_rx(void *plugin) {
  airspy *dev = (airspy *) plugin;
  pthread_mutex_lock(&dev->mutex);
  if (dev->rx_enabled) {
    pthread_mutex_unlock(&dev->mutex);
    return;
  }
  dev->ring_head = 0;
  dev->ring_count = 0;
  dev->dropped_samples = 0;
  dev->rx_enabled = true;
  pthread_mutex_unlock(&dev->mutex);

  if (dev->lib->airspy_start_rx(dev->device, airspy_rx_callback, dev) != 0) {
    pthread_mutex_lock(&dev->mutex);
    dev->rx_enabled = false;
    pthread_mutex_unlock(&dev->mutex);
  }
}

static void airspy_stop_rx(void *plugin) {
  airspy *dev = (airspy *) plugin;
  pthread_mutex_lock(&dev->mutex);
  bool was_enabled = dev->rx_enabled;
  dev->rx_enabled = false;
  // wake up airspy_process_rx
  pthread_cond_broadcast(&dev->cond);
  pthread_mutex_unlock(&dev->mutex);
  if (was_enabled) {
    // must be called without the lock: it joins the streaming thread, which can be waiting in the callback for the lock
    dev->lib->airspy_stop_rx(dev->device);
  }
}

static int airspy_set_rx_parameters(sdr_channel_config *config, void *plugin) {
  airspy *dev = (airspy *) plugin;
  if (config->frequency < AIRSPY_MIN_FREQUENCY || config->frequency > AIRSPY_MAX_FREQUENCY) {
    LOG_ERROR("frequency is outside of supported range: %" PRIu64, config->frequency);
    return -1;
  }
  if (!airspy_is_sample_rate_supported(config->sample_rate, dev)) {
    LOG_ERROR("sample_rate is not supported: %" PRIu64, config->sample_rate);
    return -1;
  }
  if ((dev->gain_mode == AIRSPY_GAIN_MODE_SENSITIVITY || dev->gain_mode == AIRSPY_GAIN_MODE_LINEARITY) &&
      !(config->gain >= 0.0F && config->gain <= AIRSPY_MAX_COMBINED_GAIN)) {
    LOG_ERROR("gain is outside of supported range: %.1f", config->gain);
    return -1;
  }
  int code;
  if (dev->current_sample_rate != config->sample_rate) {
    code = airspy_set_sample_rate(config->sample_rate, dev);
    if (code != 0) {
      return code;
    }
  }
  ERROR_CHECK(dev->lib->airspy_set_freq(dev->device, (uint32_t) config->frequency), dev);
  code = airspy_apply_gain(config->gain, dev);
  if (code != 0) {
    return code;
  }
  LOG_INFO("airspy rx: frequency=%" PRIu64 " sample_rate=%" PRIu64 " gain_mode=%s gain=%.1f", config->frequency, config->sample_rate, airspy_gain_mode_name(dev->gain_mode), config->gain);
  return 0;
}

static void airspy_destroy(void *plugin) {
  if (plugin == NULL) {
    return;
  }
  airspy *dev = (airspy *) plugin;
  if (dev->device != NULL) {
    if (dev->sync_initialized) {
      airspy_stop_rx(dev);
    }
    dev->lib->airspy_close(dev->device);
  }
  if (dev->sync_initialized) {
    pthread_cond_destroy(&dev->cond);
    pthread_mutex_destroy(&dev->mutex);
  }
  free(dev->ring);
  free(dev->output);
  free(dev->sample_rates);
  free(dev);
}

static int airspy_validate_settings(const airspy_settings *settings) {
  if (airspy_gain_mode_name(settings->gain_mode) == NULL) {
    LOG_ERROR("unknown gain mode: %d", settings->gain_mode);
    return -1;
  }
  if (settings->lna_gain > AIRSPY_MAX_LNA_GAIN) {
    LOG_ERROR("lna gain is outside of supported range [0, %d]: %u", AIRSPY_MAX_LNA_GAIN, settings->lna_gain);
    return -1;
  }
  if (settings->mixer_gain > AIRSPY_MAX_MIXER_GAIN) {
    LOG_ERROR("mixer gain is outside of supported range [0, %d]: %u", AIRSPY_MAX_MIXER_GAIN, settings->mixer_gain);
    return -1;
  }
  if (settings->vga_gain > AIRSPY_MAX_VGA_GAIN) {
    LOG_ERROR("vga gain is outside of supported range [0, %d]: %u", AIRSPY_MAX_VGA_GAIN, settings->vga_gain);
    return -1;
  }
  return 0;
}

static int airspy_load_sample_rates(airspy *dev) {
  uint32_t count = 0;
  ERROR_CHECK(dev->lib->airspy_get_samplerates(dev->device, &count, 0), dev);
  if (count == 0) {
    LOG_ERROR("airspy doesn't report any sample rates");
    return -1;
  }
  dev->sample_rates = malloc(sizeof(uint32_t) * count);
  if (dev->sample_rates == NULL) {
    return -ENOMEM;
  }
  ERROR_CHECK(dev->lib->airspy_get_samplerates(dev->device, dev->sample_rates, count), dev);
  dev->sample_rates_len = count;
  return 0;
}

int airspy_create(const airspy_settings *settings, uint32_t sdr_buffer_len, airspy_lib *lib, sdr_device **output) {
  if (sdr_buffer_len == 0 || airspy_validate_settings(settings) != 0) {
    return -1;
  }
  struct airspy_t *dev = malloc(sizeof(struct airspy_t));
  if (dev == NULL) {
    return -ENOMEM;
  }
  *dev = (struct airspy_t){0};
  dev->lib = lib;
  dev->sdr_buffer_len = sdr_buffer_len;
  dev->gain_mode = settings->gain_mode;
  dev->lna_gain = settings->lna_gain;
  dev->mixer_gain = settings->mixer_gain;
  dev->vga_gain = settings->vga_gain;
  dev->timeout_ms = settings->timeout_ms != 0 ? settings->timeout_ms : AIRSPY_DEFAULT_TIMEOUT_MS;

  dev->ring_len = (size_t) sdr_buffer_len * AIRSPY_RING_BUFFERS;
  if (dev->ring_len < AIRSPY_MIN_RING_SAMPLES) {
    dev->ring_len = AIRSPY_MIN_RING_SAMPLES;
  }
  dev->ring = malloc(sizeof(float complex) * dev->ring_len);
  dev->output = malloc(sizeof(float complex) * sdr_buffer_len);
  if (dev->ring == NULL || dev->output == NULL) {
    airspy_destroy(dev);
    return -ENOMEM;
  }
  if (pthread_mutex_init(&dev->mutex, NULL) != 0) {
    airspy_destroy(dev);
    return -1;
  }
  if (pthread_cond_init(&dev->cond, NULL) != 0) {
    pthread_mutex_destroy(&dev->mutex);
    airspy_destroy(dev);
    return -1;
  }
  dev->sync_initialized = true;

  int code;
  if (settings->serial_number == 0) {
    code = lib->airspy_open(&dev->device);
  } else {
    code = lib->airspy_open_sn(&dev->device, settings->serial_number);
  }
  if (code != AIRSPY_LIB_SUCCESS) {
    LOG_ERROR("unable to open airspy: %s (%d)", lib->airspy_error_name(code), code);
    dev->device = NULL;
    airspy_destroy(dev);
    return -1;
  }
  code = lib->airspy_set_sample_type(dev->device, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  if (code != 0) {
    airspy_destroy(dev);
    return -1;
  }
  code = airspy_load_sample_rates(dev);
  if (code != 0) {
    airspy_destroy(dev);
    return code == -ENOMEM ? code : -1;
  }
  code = lib->airspy_set_rf_bias(dev->device, settings->bias_tee ? 1 : 0);
  if (code != 0) {
    airspy_destroy(dev);
    return -1;
  }
  if (settings->sample_rate != 0) {
    code = airspy_set_sample_rate(settings->sample_rate, dev);
    if (code != 0) {
      airspy_destroy(dev);
      return -1;
    }
  }

  struct sdr_device_t *result = malloc(sizeof(struct sdr_device_t));
  if (result == NULL) {
    airspy_destroy(dev);
    return -ENOMEM;
  }
  result->plugin = dev;
  result->sdr_process_rx = airspy_process_rx;
  result->sdr_process_tx = NULL;
  result->destroy = airspy_destroy;
  result->set_rx_parameters = airspy_set_rx_parameters;
  result->set_tx_parameters = NULL;
  result->start_rx = airspy_start_rx;
  result->stop_rx = airspy_stop_rx;

  LOG_INFO("airspy: serial=%" PRIu64 " gain_mode=%s lna_gain=%u mixer_gain=%u vga_gain=%u bias_tee=%s timeout_ms=%u sample_rates=%zu",
           settings->serial_number, airspy_gain_mode_name(settings->gain_mode), settings->lna_gain, settings->mixer_gain, settings->vga_gain,
           settings->bias_tee ? "true" : "false", dev->timeout_ms, dev->sample_rates_len);
  *output = result;
  return 0;
}
