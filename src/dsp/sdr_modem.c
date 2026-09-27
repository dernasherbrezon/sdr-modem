#include "sdr_modem.h"
#include "../api_utils.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include "gfsk_modem.h"
#include "bpsk_modem.h"
#include "halfband_decim.h"
#include "psk_pm_modem.h"

// hardcoded per design: half-band decimator stop-band attenuation
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

  // optional. applied to raw I/Q before demodulate and to the modulated output before tx
  freq_offset *freq_offset;

  // optional. dumps raw I/Q samples for debugging: on rx, the (possibly freq_offset-corrected)
  // samples right before demodulate; on tx, the modulated samples right after modulate, before
  // freq_offset correction is applied
  FILE *debug_freq_offset_file;

  // optional. rx only: dumps the baseband I/Q samples right after halfband decimation
  // (i.e. right before the wrapped demodulator runs), or the freq_offset-corrected raw
  // input when no halfband decimation is configured
  FILE *debug_baseband_file;

  // which wrapped modem is in use, needed to dispatch modem-specific debug setters
  ModemRequest__ModemSettingsCase modem_settings_case;

  // sample rate seen by the wrapped modem, i.e. after halfband decimation
  uint64_t baseband_sample_rate;
};

static uint32_t modem_request_get_bandwidth(ModemRequest *req) {
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      return req->gfsk->bandwidth;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      return (uint32_t) ((1 + req->bpsk->rrc_beta) * req->bpsk->baud_rate);
    case MODEM_REQUEST__MODEM_SETTINGS_PSK_PM:
      // occupied bandwidth spans the subcarrier tone on both sides of the (suppressed) carrier,
      // plus the subcarrier's own RRC-shaped sidebands
      return 2 * (req->psk_pm->subcarrier_frequency + (uint32_t) ((1 + req->psk_pm->rrc_beta) * req->psk_pm->baud_rate));
    default:
      return 0;
  }
}

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

static int modem_halfband_decim_create(uint64_t sample_rate, uint32_t bandwidth, uint32_t max_input_buffer_length,
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
  float cutoff = ((float) bandwidth / 2.0f) / (float) *decimated_sample_rate;
  // liquid advise avoid 0 and 0.5, so put some guards here
  if (cutoff < 0.05f) {
    cutoff = 0.05f;
  } else if (cutoff > 0.45f) {
    cutoff = 0.45f;
  }
  int code = halfband_decim_create(halfband_stages, cutoff, MODEM_HALFBAND_STOPBAND_ATTENUATION_DB, max_input_buffer_length, halfband);
  if (code != 0) {
    return code;
  }
  *decimated_max_input_buffer_length = max_input_buffer_length / (1u << halfband_stages) + 1;
  return 0;
}

static int modem_create_gfsk(GfskModemSettings *req, uint64_t syncword, uint32_t syncword_bits, uint64_t sample_rate, uint32_t max_input_buffer_length, gfsk_modem **modem) {
  gfsk_modem_settings settings = {0};
  settings.rx_sample_rate = sample_rate;
  settings.tx_sample_rate = req->sample_rate;
  settings.baud_rate = req->baud_rate;
  settings.deviation = req->deviation;
  settings.bandwidth = req->bandwidth;
  settings.bt = req->bt;
  settings.use_dc_block = req->use_dc_block;
  settings.syncword = syncword;
  settings.syncword_bits = syncword_bits;
  return gfsk_modem_create(&settings, max_input_buffer_length, modem);
}

static int modem_create_bpsk_family(PskModemSettings *req, uint64_t sample_rate, psk_modem_type type, uint32_t max_input_buffer_length, bpsk_modem **modem) {
  bpsk_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = req->baud_rate;
  settings.rrc_beta = req->rrc_beta;
  settings.rrc_delay = req->rrc_delay;
  settings.costas_bandwidth = req->costas_bandwidth;
  settings.symsync_filter_bank_size = req->symsync_filter_bank_size;
  settings.bandwidth = req->bandwidth;
  settings.type = type;
  return bpsk_modem_create(&settings, max_input_buffer_length, modem);
}

static int modem_create_psk_pm(PskPmModemSettings *req, uint64_t sample_rate, uint32_t max_input_buffer_length, psk_pm_modem **modem) {
  psk_pm_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = req->baud_rate;
  settings.rrc_beta = req->rrc_beta;
  settings.rrc_delay = req->rrc_delay;
  settings.costas_bandwidth = req->costas_bandwidth;
  settings.symsync_filter_bank_size = req->symsync_filter_bank_size;
  settings.subcarrier_frequency = req->subcarrier_frequency;
  settings.modulation_index = req->modulation_index;
  settings.carrier_pll_bandwidth = req->carrier_pll_bandwidth;
  settings.subcarrier_bandwidth = req->subcarrier_bandwidth;
  return psk_pm_modem_create(&settings, max_input_buffer_length, modem);
}

int sdr_modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, sdr_modem **modem) {
  if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS__NOT_SET) {
    //do nothing, but supported
    *modem = NULL;
    return 0;
  }

  struct sdr_modem_t *result = malloc(sizeof(struct sdr_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_modem_t){0};
  int code = 0;
  uint64_t sample_rate = api_utils_get_sample_rate(req);
  uint32_t bandwidth = modem_request_get_bandwidth(req);
  uint64_t decimated_sample_rate = sample_rate;
  uint32_t decimated_buffer_length = config->buffer_size;
  code = modem_halfband_decim_create(sample_rate, bandwidth, config->buffer_size, &result->halfband, &decimated_sample_rate, &decimated_buffer_length);
  if (code != 0) {
    sdr_modem_destroy(result);
    return code;
  }
  if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_GFSK) {
    code = modem_create_gfsk(req->gfsk, req->syncword, req->syncword_bits, decimated_sample_rate, decimated_buffer_length, (gfsk_modem **) &result->modem);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
    result->modulate = gfsk_modem_modulate;
    result->demodulate = gfsk_modem_demodulate;
    result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
    result->destroy = gfsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_BPSK) {
    code = modem_create_bpsk_family(req->bpsk, decimated_sample_rate, BPSK, decimated_buffer_length, (bpsk_modem **) &result->modem);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_DPSK) {
    code = modem_create_bpsk_family(req->dpsk, decimated_sample_rate, DPSK, decimated_buffer_length, (bpsk_modem **) &result->modem);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_SDPSK) {
    code = modem_create_bpsk_family(req->sdpsk, decimated_sample_rate, SDPSK, decimated_buffer_length, (bpsk_modem **) &result->modem);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_PSK_PM) {
    code = modem_create_psk_pm(req->psk_pm, decimated_sample_rate, decimated_buffer_length, (psk_pm_modem **) &result->modem);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
    result->modulate = psk_pm_modem_modulate;
    result->demodulate = psk_pm_modem_demodulate;
    result->max_modulation_buffer_length = psk_pm_modem_max_modulation_buffer_length;
    result->destroy = psk_pm_modem_destroy;
  } else {
    fprintf(stderr, "<3>unsupported modem type: %d\n", req->modem_settings_case);
    code = -1;
  }

  if (freq_offset_file != NULL) {
    // needs to fit both the raw rx buffer and the (typically larger) modulated tx buffer, since
    // this same instance can end up being used for either direction
    size_t max_buffer_length = config->buffer_size;
    size_t max_modulation_buffer_length = result->max_modulation_buffer_length(result->modem);
    if (max_modulation_buffer_length > max_buffer_length) {
      max_buffer_length = max_modulation_buffer_length;
    }
    code = freq_offset_create(freq_offset_file, sample_rate, max_buffer_length, &result->freq_offset);
    if (code != 0) {
      sdr_modem_destroy(result);
      return code;
    }
  }

  result->modem_settings_case = req->modem_settings_case;
  result->baseband_sample_rate = decimated_sample_rate;
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
  switch (modem->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      return bpsk_modem_set_debug_constellation_file(debug_constellation_file, modem->modem);
    case MODEM_REQUEST__MODEM_SETTINGS_PSK_PM:
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
  if (modem->modem_settings_case != MODEM_REQUEST__MODEM_SETTINGS_PSK_PM) {
    // not supported by this modem type
    return 0;
  }
  return psk_pm_modem_set_debug_subcarrier_file(debug_subcarrier_file, modem->modem);
}

void sdr_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
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
