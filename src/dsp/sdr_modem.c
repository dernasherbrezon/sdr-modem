#include "sdr_modem.h"
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <liquid/liquid.h>
#include "gfsk_modem.h"
#include "bpsk_modem.h"
#include "halfband_decim.h"
#include "halfband_interp.h"
#include "psk_pm_modem.h"
#include "../log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ERROR_CHECK(x)           \
    do {                           \
      int __err_rc = (x);          \
      if (__err_rc != 0) { \
        sdr_modem_destroy(result); \
        return __err_rc;           \
      }                            \
    } while (0)

// hardcoded per design: half-band decimator/interpolator stop-band attenuation
#define MODEM_HALFBAND_STOPBAND_ATTENUATION_DB 60.0f
// half-band decimator: keep the decimated rate at least this many times the signal bandwidth
// so that the half-band filter's own transition band does not clip the signal
#define MODEM_HALFBAND_MIN_OVERSAMPLE 2.0f
#define MODEM_HALFBAND_MAX_STAGES 6

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

  // pushes the tail of the modulated signal out and resets the tx state
  void (*modulate_flush)(float complex **output, size_t *output_len, void *modem);

  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  size_t (*max_modulation_buffer_length)(void *modem);

  float (*get_frequency_offset)(void *modem);

  void (*destroy)(void *modem);

  // optional. decimates raw I/Q down to just above the signal bandwidth before demodulate, so the
  // wrapped modem's own DSP chain runs at a lower, cheaper sample rate
  halfband_decim *halfband;

  // optional. present whenever halfband is: interpolates the wrapped modem's modulated output back up
  // to the raw sample rate, so tx runs at the same rate as rx
  halfband_interp *halfband_tx;

  // optional. applied to raw I/Q before demodulate and to the modulated output before tx
  freq_offset *freq_offset;

  // optional. dumps raw I/Q samples for debugging: on rx, the (possibly freq_offset-corrected)
  // samples right before demodulate; on tx, the modulated samples right after modulate and halfband
  // interpolation, before freq_offset correction is applied
  FILE *debug_freq_offset_file;

  // optional. rx only: dumps the baseband I/Q samples right before the wrapped demodulator runs: after halfband
  // decimation (or the freq_offset-corrected raw input when no halfband decimation is configured) and after
  // the carrier correction
  FILE *debug_baseband_file;

  // rx only. runs on the baseband samples (after halfband decimation, before the carrier correction). modulation independent
  carrier_detector *carrier_detector;

  // rx only. mixes the baseband down by the frequency of the detected carrier, so that the wrapped modem sees the carrier
  // at the center and its own loops only track what is left. used from the first detected carrier on: not applied
  // (and carrier_offset is 0) until then. stays on after the end of the transmission until the next carrier is detected
  nco_crcf carrier_nco;
  float complex *carrier_nco_output;
  size_t carrier_nco_output_len;
  // Hz, relative to the center. what carrier_nco removes
  float carrier_offset;

  void (*callback)(sdr_modem_event_type event_type, void *ctx);

  void *callback_ctx;

  // which wrapped modem is in use (MODEM_TYPE_*), needed to dispatch modem-specific debug setters
  sdr_modem_type modem_type;

  // true when something was modulated since the creation or the last flush
  bool tx_pending;

  // sample rate after halfband decimation
  uint64_t baseband_sample_rate;
};

static unsigned int sdr_modem_estimate_halfband_stages(uint64_t sample_rate, uint32_t bandwidth) {
  if (bandwidth == 0) {
    return 0;
  }
  uint64_t required_rate = (uint64_t) ceilf(MODEM_HALFBAND_MIN_OVERSAMPLE * (float) bandwidth);
  unsigned int num_stages = 0;
  while (num_stages < MODEM_HALFBAND_MAX_STAGES && (sample_rate >> (num_stages + 1)) >= required_rate) {
    num_stages++;
  }
  return num_stages;
}

// normalized to the decimated sample rate
static float sdr_modem_halfband_cutoff(uint32_t bandwidth, uint64_t decimated_sample_rate) {
  float cutoff = ((float) bandwidth / 2.0f) / (float) decimated_sample_rate;
  // liquid advise avoid 0 and 0.5, so put some guards here
  if (cutoff < 0.05f) {
    cutoff = 0.05f;
  } else if (cutoff > 0.45f) {
    cutoff = 0.45f;
  }
  return cutoff;
}

static int sdr_modem_get_bandwidth(sdr_modem_type modem_type, const sdr_modem_settings *settings, uint32_t *bandwidth) {
  switch (modem_type) {
    case MODEM_TYPE_GFSK:
      *bandwidth = settings->modem.gfsk.bandwidth;
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      *bandwidth = (uint32_t) ((1 + settings->modem.psk.rrc_beta) * settings->modem.psk.baud_rate);
      break;
    case MODEM_TYPE_PSK_PM:
      // occupied bandwidth spans the subcarrier tone on both sides of the (suppressed) carrier,
      // plus the subcarrier's own RRC-shaped sidebands
      *bandwidth = 2 * (settings->modem.psk_pm.subcarrier_frequency + (uint32_t) ((1 + settings->modem.psk_pm.rrc_beta) * settings->modem.psk_pm.baud_rate));
      break;
    default:
      LOG_ERROR("unsupported modem type: %d", modem_type);
      return -1;
  }
  return 0;
}

static int sdr_modem_get_sample_date(sdr_modem_type modem_type, const sdr_modem_settings *settings, uint64_t *sample_rate) {
  switch (modem_type) {
    case MODEM_TYPE_GFSK:
      *sample_rate = settings->modem.gfsk.sample_rate;
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      *sample_rate = settings->modem.psk.sample_rate;
      break;
    case MODEM_TYPE_PSK_PM:
      *sample_rate = settings->modem.psk_pm.sample_rate;
      break;
    default:
      LOG_ERROR("unsupported modem type: %d", modem_type);
      return -1;
  }
  return 0;
}

static int sdr_modem_halfband_decim_create(uint32_t bandwidth, uint64_t sample_rate, uint32_t max_input_buffer_length,
                                           halfband_decim **halfband, uint64_t *decimated_sample_rate,
                                           uint32_t *decimated_max_input_buffer_length) {
  *halfband = NULL;
  *decimated_sample_rate = sample_rate;
  *decimated_max_input_buffer_length = max_input_buffer_length;

  unsigned int halfband_stages = sdr_modem_estimate_halfband_stages(sample_rate, bandwidth);
  if (halfband_stages == 0) {
    return 0;
  }

  *decimated_sample_rate = sample_rate >> halfband_stages;
  float cutoff = sdr_modem_halfband_cutoff(bandwidth, *decimated_sample_rate);
  int code = halfband_decim_create(halfband_stages, cutoff, MODEM_HALFBAND_STOPBAND_ATTENUATION_DB, max_input_buffer_length, halfband);
  if (code != 0) {
    return code;
  }
  *decimated_max_input_buffer_length = max_input_buffer_length / (1u << halfband_stages) + 1;
  return 0;
}

// called by the carrier detector from sdr_modem_demodulate, before the buffer which contains the carrier is corrected
static void sdr_modem_on_carrier(carrier_detector_event_type event_type, float frequency_offset, void *ctx) {
  sdr_modem *modem = (sdr_modem *) ctx;
  if (event_type == CARRIER_DETECTED) {
    modem->carrier_offset = frequency_offset;
    nco_crcf_set_frequency(modem->carrier_nco, (float) (2.0 * M_PI * (double) frequency_offset / (double) modem->baseband_sample_rate));
    if (modem->callback != NULL) {
      modem->callback(MODEM_EVENT_TYPE_CARRIER_DETECTED, modem->callback_ctx);
    }
  } else if (event_type == CARRIER_LOST) {
    if (modem->callback != NULL) {
      modem->callback(MODEM_EVENT_TYPE_CARRIER_LOST, modem->callback_ctx);
    }
  }
}

// the nco phase is continuous across buffers and across frequency updates
static void sdr_modem_correct_carrier(const float complex **input, size_t input_len, sdr_modem *modem) {
  if (modem->carrier_offset == 0.0f || *input == NULL) {
    return;
  }
  if (input_len > modem->carrier_nco_output_len) {
    LOG_ERROR("carrier correction skipped. requested buffer %zu is more than max: %zu", input_len, modem->carrier_nco_output_len);
    return;
  }
  nco_crcf_mix_block_down(modem->carrier_nco, (float complex *) *input, modem->carrier_nco_output, (unsigned int) input_len);
  *input = modem->carrier_nco_output;
}

int sdr_modem_create(const sdr_modem_settings *settings, uint32_t buffer_size, const char *freq_offset_file, sdr_modem **modem) {
  struct sdr_modem_t *result = malloc(sizeof(struct sdr_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_modem_t){0};
  result->modem_type = settings->modem_type;

  uint32_t bandwidth;
  ERROR_CHECK(sdr_modem_get_bandwidth(settings->modem_type, settings, &bandwidth));
  uint64_t sample_rate;
  ERROR_CHECK(sdr_modem_get_sample_date(settings->modem_type, settings, &sample_rate));
  uint32_t decimated_buffer_length = buffer_size;
  ERROR_CHECK(sdr_modem_halfband_decim_create(bandwidth, sample_rate, buffer_size, &result->halfband, &result->baseband_sample_rate, &decimated_buffer_length));

  int code = 0;
  switch (settings->modem_type) {
    case MODEM_TYPE_GFSK: {
      gfsk_modem_settings decimated = settings->modem.gfsk;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = gfsk_modem_modulate;
      result->modulate_flush = gfsk_modem_modulate_flush;
      result->demodulate = gfsk_modem_demodulate;
      result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
      result->get_frequency_offset = gfsk_modem_get_frequency_offset;
      result->destroy = gfsk_modem_destroy;
      code = gfsk_modem_create(&decimated, buffer_size, (gfsk_modem **) &result->modem);
      break;
    }
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK: {
      bpsk_modem_settings decimated = settings->modem.psk;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = bpsk_modem_modulate;
      result->modulate_flush = bpsk_modem_modulate_flush;
      result->demodulate = bpsk_modem_demodulate;
      result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
      result->get_frequency_offset = bpsk_modem_get_frequency_offset;
      result->destroy = bpsk_modem_destroy;
      code = bpsk_modem_create(&decimated, buffer_size, (bpsk_modem **) &result->modem);
      break;
    }
    case MODEM_TYPE_PSK_PM: {
      psk_pm_modem_settings decimated = settings->modem.psk_pm;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = psk_pm_modem_modulate;
      result->modulate_flush = psk_pm_modem_modulate_flush;
      result->demodulate = psk_pm_modem_demodulate;
      result->max_modulation_buffer_length = psk_pm_modem_max_modulation_buffer_length;
      result->get_frequency_offset = psk_pm_modem_get_frequency_offset;
      result->destroy = psk_pm_modem_destroy;
      code = psk_pm_modem_create(&decimated, buffer_size, (psk_pm_modem **) &result->modem);
      break;
    }
    default:
      code = -1;
  }
  if (code != 0) {
    sdr_modem_destroy(result);
    return code;
  }

  if (result->halfband != NULL) {
    // mirror of the rx decimation: same number of stages and the same cutoff
    unsigned int halfband_stages = sdr_modem_estimate_halfband_stages(sample_rate, bandwidth);
    uint32_t max_modulation_buffer_length = (uint32_t) result->max_modulation_buffer_length(result->modem);
    ERROR_CHECK(halfband_interp_create(halfband_stages, sdr_modem_halfband_cutoff(bandwidth, result->baseband_sample_rate), MODEM_HALFBAND_STOPBAND_ATTENUATION_DB, max_modulation_buffer_length, &result->halfband_tx));
  }

  carrier_detector_settings carrier_settings = carrier_detector_default_settings(result->baseband_sample_rate);
  ERROR_CHECK(carrier_detector_create(&carrier_settings, &result->carrier_detector));
  carrier_detector_set_callback(sdr_modem_on_carrier, result, result->carrier_detector);
  result->carrier_nco = nco_crcf_create(LIQUID_NCO);
  if (result->carrier_nco == NULL) {
    sdr_modem_destroy(result);
    return -ENOMEM;
  }
  result->carrier_nco_output_len = decimated_buffer_length;
  result->carrier_nco_output = malloc(sizeof(float complex) * result->carrier_nco_output_len);
  if (result->carrier_nco_output == NULL) {
    sdr_modem_destroy(result);
    return -ENOMEM;
  }

  if (freq_offset_file != NULL) {
    // needs to fit both the raw rx buffer and the (typically larger) modulated tx buffer, since
    // this same instance can end up being used for either direction
    size_t max_buffer_length = buffer_size;
    size_t max_modulation_buffer_length = sdr_modem_max_modulation_buffer_length(result);
    if (max_modulation_buffer_length > max_buffer_length) {
      max_buffer_length = max_modulation_buffer_length;
    }
    ERROR_CHECK(freq_offset_create(freq_offset_file, sample_rate, max_buffer_length, &result->freq_offset));
  }

  *modem = result;
  return 0;
}

static int sdr_modem_reopen_debug_file(FILE **file, const char *path, const char *name) {
  if (*file != NULL) {
    fclose(*file);
    *file = NULL;
  }
  if (path == NULL) {
    return 0;
  }
  *file = fopen(path, "wb");
  if (*file == NULL) {
    LOG_ERROR("unable to open debug %s file: %s", name, path);
    return -1;
  }
  return 0;
}

int sdr_modem_set_debug_freq_offset_file(const char *debug_freq_offset_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  return sdr_modem_reopen_debug_file(&modem->debug_freq_offset_file, debug_freq_offset_file, "freq offset");
}

int sdr_modem_set_debug_baseband_file(const char *debug_baseband_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  int code = sdr_modem_reopen_debug_file(&modem->debug_baseband_file, debug_baseband_file, "baseband");
  if (code == 0 && debug_baseband_file != NULL) {
    LOG_DEBUG("baseband sample rate: %"PRIu64, modem->baseband_sample_rate);
  }
  return code;
}

int sdr_modem_set_debug_constellation_file(const char *debug_constellation_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  switch (modem->modem_type) {
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      return bpsk_modem_set_debug_constellation_file(debug_constellation_file, modem->modem);
    case MODEM_TYPE_PSK_PM:
      return psk_pm_modem_set_debug_constellation_file(debug_constellation_file, modem->modem);
    default:
      // not supported by this modem type
      return 0;
  }
}

int sdr_modem_set_debug_subcarrier_file(const char *debug_subcarrier_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  if (modem->modem_type != MODEM_TYPE_PSK_PM) {
    // not supported by this modem type
    return 0;
  }
  return psk_pm_modem_set_debug_subcarrier_file(debug_subcarrier_file, modem->modem);
}

// debug dump and frequency offset correction
static void sdr_modem_tx_postprocess(float complex **output, size_t *output_len, sdr_modem *modem) {
  if (modem->debug_freq_offset_file != NULL && *output != NULL) {
    fwrite(*output, sizeof(float complex), *output_len, modem->debug_freq_offset_file);
  }
  if (modem->freq_offset != NULL && *output != NULL) {
    freq_offset_process(*output, *output_len, output, output_len, modem->freq_offset);
  }
}

void sdr_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
  if (*output == NULL) {
    return;
  }
  if (input_len > 0) {
    modem->tx_pending = true;
  }
  if (modem->halfband_tx != NULL) {
    // streaming: the filter keeps its history for the next call, the tail comes with sdr_modem_modulate_flush
    halfband_interp_process(*output, *output_len, output, output_len, modem->halfband_tx);
  }
  sdr_modem_tx_postprocess(output, output_len, modem);
}

void sdr_modem_modulate_flush(float complex **output, size_t *output_len, sdr_modem *modem) {
  if (!modem->tx_pending) {
    *output = NULL;
    *output_len = 0;
    return;
  }
  modem->modulate_flush(output, output_len, modem->modem);
  if (*output == NULL) {
    return;
  }
  if (modem->halfband_tx != NULL) {
    halfband_interp_process_flush(*output, *output_len, output, output_len, modem->halfband_tx);
    halfband_interp_reset(modem->halfband_tx);
  }
  sdr_modem_tx_postprocess(output, output_len, modem);
  modem->tx_pending = false;
}

void sdr_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem) {
  if (modem->freq_offset != NULL) {
    float complex *corrected = NULL;
    size_t corrected_len = 0;
    freq_offset_process(input, input_len, &corrected, &corrected_len, modem->freq_offset);
    input = corrected;
    input_len = corrected_len;
  }
  if (modem->debug_freq_offset_file != NULL) {
    fwrite(input, sizeof(float complex), input_len, modem->debug_freq_offset_file);
  }
  if (modem->halfband != NULL) {
    float complex *halfband_output = NULL;
    size_t halfband_output_len = 0;
    halfband_decim_process(input, input_len, &halfband_output, &halfband_output_len, modem->halfband);
    input = halfband_output;
    input_len = halfband_output_len;
  }
  // the detector must see the signal as it is: the correction below follows its result
  carrier_detector_process(input, input_len, modem->carrier_detector);
  sdr_modem_correct_carrier(&input, input_len, modem);
  if (modem->debug_baseband_file != NULL) {
    fwrite(input, sizeof(float complex), input_len, modem->debug_baseband_file);
  }
  modem->demodulate(input, input_len, output, output_len, modem->modem);
}

float sdr_modem_get_frequency_offset(sdr_modem *modem) {
  if (modem == NULL || modem->modem == NULL) {
    return 0.0f;
  }
  // the wrapped modem sees the signal after the carrier correction, so it tracks only the remainder
  return modem->carrier_offset + modem->get_frequency_offset(modem->modem);
}

void sdr_modem_set_callback(void (*callback)(sdr_modem_event_type event_type, void *ctx), void *ctx, sdr_modem *modem) {
  if (modem == NULL) {
    return;
  }
  modem->callback = callback;
  modem->callback_ctx = ctx;
}

size_t sdr_modem_max_modulation_buffer_length(sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  if (modem->halfband_tx != NULL) {
    return halfband_interp_max_output_buffer_length(modem->halfband_tx);
  }
  return modem->max_modulation_buffer_length(modem->modem);
}

void sdr_modem_destroy(sdr_modem *modem) {
  if (modem == NULL) {
    return;
  }
  if (modem->modem != NULL) {
    modem->destroy(modem->modem);
  }
  if (modem->halfband != NULL) {
    halfband_decim_destroy(modem->halfband);
  }
  if (modem->halfband_tx != NULL) {
    halfband_interp_destroy(modem->halfband_tx);
  }
  if (modem->freq_offset != NULL) {
    freq_offset_destroy(modem->freq_offset);
  }
  if (modem->carrier_detector != NULL) {
    carrier_detector_destroy(modem->carrier_detector);
  }
  if (modem->carrier_nco != NULL) {
    nco_crcf_destroy(modem->carrier_nco);
  }
  free(modem->carrier_nco_output);
  if (modem->debug_freq_offset_file != NULL) {
    fclose(modem->debug_freq_offset_file);
  }
  if (modem->debug_baseband_file != NULL) {
    fclose(modem->debug_baseband_file);
  }
  free(modem);
}
