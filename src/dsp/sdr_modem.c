#include "sdr_modem.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include "gfsk_modem.h"
#include "bpsk_modem.h"
#include "halfband_decim.h"
#include "halfband_interp.h"
#include "psk_pm_modem.h"

// hardcoded per design: half-band decimator/interpolator stop-band attenuation
#define MODEM_HALFBAND_STOPBAND_ATTENUATION_DB 60.0f
// half-band decimator: keep the decimated rate at least this many times the signal bandwidth
// so that the half-band filter's own transition band does not clip the signal
#define MODEM_HALFBAND_MIN_OVERSAMPLE 2.0f
#define MODEM_HALFBAND_MAX_STAGES 6

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  size_t (*max_modulation_buffer_length)(void *modem);

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

  // optional. rx only: dumps the baseband I/Q samples right after halfband decimation
  // (i.e. right before the wrapped demodulator runs), or the freq_offset-corrected raw
  // input when no halfband decimation is configured
  FILE *debug_baseband_file;

  // which wrapped modem is in use (MODEM_TYPE_*), needed to dispatch modem-specific debug setters
  int modem_type;

  // sample rate after halfband decimation
  uint64_t baseband_sample_rate;
};

static unsigned int modem_estimate_halfband_stages(uint64_t sample_rate, uint32_t bandwidth) {
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
static float modem_halfband_cutoff(uint32_t bandwidth, uint64_t decimated_sample_rate) {
  float cutoff = ((float) bandwidth / 2.0f) / (float) decimated_sample_rate;
  // liquid advise avoid 0 and 0.5, so put some guards here
  if (cutoff < 0.05f) {
    cutoff = 0.05f;
  } else if (cutoff > 0.45f) {
    cutoff = 0.45f;
  }
  return cutoff;
}

static int modem_get_bandwidth(int modem_type, const sdr_modem_settings *settings, uint32_t *bandwidth) {
  switch (modem_type) {
    case MODEM_TYPE_GFSK:
      *bandwidth = settings->gfsk.bandwidth;
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      *bandwidth = (uint32_t) ((1 + settings->psk.rrc_beta) * settings->psk.baud_rate);
      break;
    case MODEM_TYPE_PSK_PM:
      // occupied bandwidth spans the subcarrier tone on both sides of the (suppressed) carrier,
      // plus the subcarrier's own RRC-shaped sidebands
      *bandwidth = 2 * (settings->psk_pm.subcarrier_frequency + (uint32_t) ((1 + settings->psk_pm.rrc_beta) * settings->psk_pm.baud_rate));
      break;
    default:
      fprintf(stderr, "<3>unsupported modem type: %d\n", modem_type);
      return -1;
  }
  return 0;
}

static int modem_get_sample_date(int modem_type, const sdr_modem_settings *settings, uint64_t *sample_rate) {
  switch (modem_type) {
    case MODEM_TYPE_GFSK:
      *sample_rate = settings->gfsk.sample_rate;
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      *sample_rate = settings->psk.sample_rate;
      break;
    case MODEM_TYPE_PSK_PM:
      *sample_rate = settings->psk_pm.sample_rate;
      break;
    default:
      fprintf(stderr, "<3>unsupported modem type: %d\n", modem_type);
      return -1;
  }
  return 0;
}

static int modem_halfband_decim_create(uint32_t bandwidth, uint64_t sample_rate, uint32_t max_input_buffer_length,
                                       halfband_decim **halfband, uint64_t *decimated_sample_rate,
                                       uint32_t *decimated_max_input_buffer_length) {
  *halfband = NULL;
  *decimated_sample_rate = sample_rate;
  *decimated_max_input_buffer_length = max_input_buffer_length;

  unsigned int halfband_stages = modem_estimate_halfband_stages(sample_rate, bandwidth);
  if (halfband_stages == 0) {
    return 0;
  }

  *decimated_sample_rate = sample_rate >> halfband_stages;
  float cutoff = modem_halfband_cutoff(bandwidth, *decimated_sample_rate);
  int code = halfband_decim_create(halfband_stages, cutoff, MODEM_HALFBAND_STOPBAND_ATTENUATION_DB, max_input_buffer_length, halfband);
  if (code != 0) {
    return code;
  }
  *decimated_max_input_buffer_length = max_input_buffer_length / (1u << halfband_stages) + 1;
  return 0;
}

int sdr_modem_create(int modem_type, const sdr_modem_settings *settings, uint32_t buffer_size, const char *freq_offset_file, sdr_modem **modem) {
  struct sdr_modem_t *result = malloc(sizeof(struct sdr_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_modem_t){0};
  result->modem_type = modem_type;

  uint32_t bandwidth;
  int code = modem_get_bandwidth(modem_type, settings, &bandwidth);
  if (code != 0) {
    sdr_modem_destroy(result);
    return code;
  }
  uint64_t sample_rate;
  code = modem_get_sample_date(modem_type, settings, &sample_rate);
  if (code != 0) {
    sdr_modem_destroy(result);
    return code;
  }

  uint32_t decimated_buffer_length = buffer_size;
  code = modem_halfband_decim_create(bandwidth, sample_rate, buffer_size, &result->halfband, &result->baseband_sample_rate, &decimated_buffer_length);
  if (code != 0) {
    sdr_modem_destroy(result);
    return code;
  }
  switch (modem_type) {
    case MODEM_TYPE_GFSK: {
      gfsk_modem_settings decimated = settings->gfsk;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = gfsk_modem_modulate;
      result->demodulate = gfsk_modem_demodulate;
      result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
      result->destroy = gfsk_modem_destroy;
      code = gfsk_modem_create(&decimated, buffer_size, (gfsk_modem **) &result->modem);
      break;
    }
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK: {
      bpsk_modem_settings decimated = settings->psk;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = bpsk_modem_modulate;
      result->demodulate = bpsk_modem_demodulate;
      result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
      result->destroy = bpsk_modem_destroy;
      code = bpsk_modem_create(&decimated, buffer_size, (bpsk_modem **) &result->modem);
      break;
    }
    case MODEM_TYPE_PSK_PM: {
      psk_pm_modem_settings decimated = settings->psk_pm;
      decimated.sample_rate = result->baseband_sample_rate;
      result->modulate = psk_pm_modem_modulate;
      result->demodulate = psk_pm_modem_demodulate;
      result->max_modulation_buffer_length = psk_pm_modem_max_modulation_buffer_length;
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
    unsigned int halfband_stages = modem_estimate_halfband_stages(sample_rate, bandwidth);
    uint32_t max_modulation_buffer_length = (uint32_t) result->max_modulation_buffer_length(result->modem);
    code = halfband_interp_create(halfband_stages, modem_halfband_cutoff(bandwidth, result->baseband_sample_rate), MODEM_HALFBAND_STOPBAND_ATTENUATION_DB, max_modulation_buffer_length, &result->halfband_tx);
    if (code != 0) {
      return code;
    }
  }

  if (freq_offset_file != NULL) {
    // needs to fit both the raw rx buffer and the (typically larger) modulated tx buffer, since
    // this same instance can end up being used for either direction
    size_t max_buffer_length = buffer_size;
    size_t max_modulation_buffer_length = sdr_modem_max_modulation_buffer_length(result);
    if (max_modulation_buffer_length > max_buffer_length) {
      max_buffer_length = max_modulation_buffer_length;
    }
    code = freq_offset_create(freq_offset_file, sample_rate, max_buffer_length, &result->freq_offset);
    if (code != 0) {
      return code;
    }
  }

  *modem = result;
  return 0;
}

static int modem_reopen_debug_file(FILE **file, const char *path, const char *name) {
  if (*file != NULL) {
    fclose(*file);
    *file = NULL;
  }
  if (path == NULL) {
    return 0;
  }
  *file = fopen(path, "wb");
  if (*file == NULL) {
    fprintf(stderr, "<3>unable to open debug %s file: %s\n", name, path);
    return -1;
  }
  return 0;
}

int sdr_modem_set_debug_freq_offset_file(const char *debug_freq_offset_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  return modem_reopen_debug_file(&modem->debug_freq_offset_file, debug_freq_offset_file, "freq offset");
}

int sdr_modem_set_debug_baseband_file(const char *debug_baseband_file, sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  int code = modem_reopen_debug_file(&modem->debug_baseband_file, debug_baseband_file, "baseband");
  if (code == 0 && debug_baseband_file != NULL) {
    fprintf(stdout, "baseband sample rate: %"PRIu64"\n", modem->baseband_sample_rate);
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

void sdr_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
  if (modem->halfband_tx != NULL && *output != NULL) {
    halfband_interp_process(*output, *output_len, output, output_len, modem->halfband_tx);
  }
  if (modem->debug_freq_offset_file != NULL && *output != NULL) {
    fwrite(*output, sizeof(float complex), *output_len, modem->debug_freq_offset_file);
  }
  if (modem->freq_offset != NULL && *output != NULL) {
    freq_offset_process(*output, *output_len, output, output_len, modem->freq_offset);
  }
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
  if (modem->debug_baseband_file != NULL) {
    fwrite(input, sizeof(float complex), input_len, modem->debug_baseband_file);
  }
  modem->demodulate(input, input_len, output, output_len, modem->modem);
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
  if (modem->debug_freq_offset_file != NULL) {
    fclose(modem->debug_freq_offset_file);
  }
  if (modem->debug_baseband_file != NULL) {
    fclose(modem->debug_baseband_file);
  }
  free(modem);
}
