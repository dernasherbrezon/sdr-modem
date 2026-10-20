#include "airspy.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include "../log.h"
#include "../queue.h"

// supported by r820t tuner
#define AIRSPY_MIN_FREQUENCY 24000000ULL
#define AIRSPY_MAX_FREQUENCY 1750000000ULL
// the queue is not blocking: when it is full the newest buffer is overwritten
#define AIRSPY_QUEUE_SIZE 16

struct airspy_t {
  airspy_lib *lib;
  struct airspy_device *device;

  airspy_gain_mode gain_mode;
  uint8_t lna_gain;
  uint8_t mixer_gain;
  uint8_t vga_gain;

  uint32_t *sample_rates;
  size_t sample_rates_len;
  // 0 - unknown
  uint64_t current_sample_rate;

  size_t sdr_buffer_len;
  // filled by the libairspy streaming thread and drained by airspy_process_rx.
  // Each message is exactly sdr_buffer_len samples. The queue is interrupted (poison pill) until start_rx and after stop_rx
  queue *queue;
  bool rx_started;
  // device blocks are accumulated here until sdr_buffer_len samples are collected. Used only by the libairspy streaming thread
  float complex *staging;
  size_t staging_len;
  // copy of the buffer taken from the queue
  float complex *output;
};

static int airspy_check(int code, const char *what, airspy *dev) {
  if (code != AIRSPY_LIB_SUCCESS) {
    LOG_ERROR("unable to %s: %s (%d)", what, dev->lib->airspy_error_name(code), code);
    return -1;
  }
  return 0;
}

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
  if (transfer->sample_type != AIRSPY_LIB_SAMPLE_FLOAT32_IQ || transfer->sample_count <= 0) {
    return 0;
  }
  const float complex *input = (const float complex *) transfer->samples;
  size_t remaining = (size_t) transfer->sample_count;
  while (remaining > 0) {
    size_t len = dev->sdr_buffer_len - dev->staging_len;
    if (len > remaining) {
      len = remaining;
    }
    memcpy(dev->staging + dev->staging_len, input, len * sizeof(float complex));
    dev->staging_len += len;
    input += len;
    remaining -= len;
    if (dev->staging_len == dev->sdr_buffer_len) {
      queue_message message = {
        .buffer = dev->staging,
        .buffer_len = dev->sdr_buffer_len * sizeof(float complex),
      };
      queue_put(&message, dev->queue);
      dev->staging_len = 0;
    }
  }
  return 0;
}

static int airspy_process_rx(float complex **output, size_t *output_len, void *plugin) {
  airspy *dev = (airspy *) plugin;
  *output = NULL;
  *output_len = 0;
  queue_message message;
  queue_take(&message, dev->queue);
  if (message.buffer == NULL) {
    // poison pill: rx is not started or was stopped and everything was read
    return -1;
  }
  memcpy(dev->output, message.buffer, message.buffer_len);
  queue_complete(dev->queue);
  *output = dev->output;
  *output_len = dev->sdr_buffer_len;
  return 0;
}

static int airspy_set_agc(airspy *dev, uint8_t enabled) {
  int code = airspy_check(dev->lib->airspy_set_lna_agc(dev->device, enabled), "set lna agc", dev);
  if (code != 0) {
    return code;
  }
  return airspy_check(dev->lib->airspy_set_mixer_agc(dev->device, enabled), "set mixer agc", dev);
}

static int airspy_apply_gain(float gain, airspy *dev) {
  int code;
  switch (dev->gain_mode) {
    case AIRSPY_GAIN_MODE_AUTO:
      code = airspy_set_agc(dev, 1);
      if (code != 0) {
        return code;
      }
      return airspy_check(dev->lib->airspy_set_vga_gain(dev->device, dev->vga_gain), "set vga gain", dev);
    case AIRSPY_GAIN_MODE_SENSITIVITY:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      return airspy_check(dev->lib->airspy_set_sensitivity_gain(dev->device, (uint8_t) lroundf(gain)), "set sensitivity gain", dev);
    case AIRSPY_GAIN_MODE_LINEARITY:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      return airspy_check(dev->lib->airspy_set_linearity_gain(dev->device, (uint8_t) lroundf(gain)), "set linearity gain", dev);
    case AIRSPY_GAIN_MODE_MANUAL:
      code = airspy_set_agc(dev, 0);
      if (code != 0) {
        return code;
      }
      code = airspy_check(dev->lib->airspy_set_lna_gain(dev->device, dev->lna_gain), "set lna gain", dev);
      if (code != 0) {
        return code;
      }
      code = airspy_check(dev->lib->airspy_set_mixer_gain(dev->device, dev->mixer_gain), "set mixer gain", dev);
      if (code != 0) {
        return code;
      }
      return airspy_check(dev->lib->airspy_set_vga_gain(dev->device, dev->vga_gain), "set vga gain", dev);
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
  int code = airspy_check(dev->lib->airspy_set_samplerate(dev->device, (uint32_t) sample_rate), "set sample rate", dev);
  if (code != 0) {
    return code;
  }
  dev->current_sample_rate = sample_rate;
  return 0;
}

// start_rx, stop_rx and process_rx are expected to be called from the same thread, except stop_rx which can be also called during shutdown
static void airspy_start_rx(void *plugin) {
  airspy *dev = (airspy *) plugin;
  if (dev->rx_started) {
    return;
  }
  // discard data from the previous session and remove the poison pill
  queue_reset(dev->queue);
  dev->staging_len = 0;
  if (airspy_check(dev->lib->airspy_start_rx(dev->device, airspy_rx_callback, dev), "start rx", dev) != 0) {
    // do not let airspy_process_rx wait for the data which will never come
    queue_interrupt(dev->queue);
    return;
  }
  dev->rx_started = true;
}

static void airspy_stop_rx(void *plugin) {
  airspy *dev = (airspy *) plugin;
  // wake up airspy_process_rx. It returns the remaining buffers first
  queue_interrupt(dev->queue);
  if (dev->rx_started) {
    dev->rx_started = false;
    airspy_check(dev->lib->airspy_stop_rx(dev->device), "stop rx", dev);
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
  code = airspy_check(dev->lib->airspy_set_freq(dev->device, (uint32_t) config->frequency), "set frequency", dev);
  if (code != 0) {
    return code;
  }
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
    airspy_stop_rx(dev);
    dev->lib->airspy_close(dev->device);
  }
  if (dev->queue != NULL) {
    destroy_queue(dev->queue);
  }
  if (dev->staging != NULL) {
    free(dev->staging);
  }
  if (dev->output != NULL) {
    free(dev->output);
  }
  if (dev->sample_rates != NULL) {
    free(dev->sample_rates);
  }
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
  int code = airspy_check(dev->lib->airspy_get_samplerates(dev->device, &count, 0), "get number of sample rates", dev);
  if (code != 0) {
    return code;
  }
  if (count == 0) {
    LOG_ERROR("airspy doesn't report any sample rates");
    return -1;
  }
  dev->sample_rates = malloc(sizeof(uint32_t) * count);
  if (dev->sample_rates == NULL) {
    return -ENOMEM;
  }
  code = airspy_check(dev->lib->airspy_get_samplerates(dev->device, dev->sample_rates, count), "get sample rates", dev);
  if (code != 0) {
    return code;
  }
  dev->sample_rates_len = count;
  return 0;
}

int airspy_create(const airspy_settings *settings, uint32_t sdr_buffer_len, airspy_lib *lib, sdr_device **output) {
  if (sdr_buffer_len == 0 || sdr_buffer_len > UINT32_MAX / sizeof(float complex) || airspy_validate_settings(settings) != 0) {
    return -1;
  }
  struct airspy_t *dev = malloc(sizeof(struct airspy_t));
  if (dev == NULL) {
    return -ENOMEM;
  }
  *dev = (struct airspy_t){0};
  dev->lib = lib;
  dev->gain_mode = settings->gain_mode;
  dev->lna_gain = settings->lna_gain;
  dev->mixer_gain = settings->mixer_gain;
  dev->vga_gain = settings->vga_gain;

  dev->sdr_buffer_len = sdr_buffer_len;
  dev->staging = malloc(sizeof(float complex) * sdr_buffer_len);
  dev->output = malloc(sizeof(float complex) * sdr_buffer_len);
  if (dev->staging == NULL || dev->output == NULL) {
    airspy_destroy(dev);
    return -ENOMEM;
  }
  int code = create_queue(sizeof(float complex) * sdr_buffer_len, AIRSPY_QUEUE_SIZE, false, &dev->queue);
  if (code != 0) {
    airspy_destroy(dev);
    return code;
  }

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
  code = airspy_check(lib->airspy_set_sample_type(dev->device, AIRSPY_LIB_SAMPLE_FLOAT32_IQ), "set sample type", dev);
  if (code != 0) {
    airspy_destroy(dev);
    return -1;
  }
  code = airspy_load_sample_rates(dev);
  if (code != 0) {
    airspy_destroy(dev);
    return -1;
  }
  code = airspy_check(lib->airspy_set_rf_bias(dev->device, settings->bias_tee ? 1 : 0), "set bias tee", dev);
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

  LOG_INFO("airspy: serial=%" PRIu64 " gain_mode=%s lna_gain=%u mixer_gain=%u vga_gain=%u bias_tee=%s sample_rates=%zu",
           settings->serial_number, airspy_gain_mode_name(settings->gain_mode), settings->lna_gain, settings->mixer_gain, settings->vga_gain,
           settings->bias_tee ? "true" : "false", dev->sample_rates_len);
  *output = result;
  return 0;
}
