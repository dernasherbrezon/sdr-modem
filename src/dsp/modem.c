#include "modem.h"
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include "gfsk_modem.h"
#include "bpsk_modem.h"

// hardcoded per design: half-band decimator stop-band attenuation
#define MODEM_HALFBAND_STOPBAND_ATTENUATION_DB 60.0f
// half-band decimator: keep the decimated rate at least this many times the signal bandwidth
// so that the half-band filter's own transition band does not clip the signal
#define MODEM_HALFBAND_MIN_OVERSAMPLE 2.0f
#define MODEM_HALFBAND_MAX_STAGES 6

static uint64_t modem_get_sample_rate(ModemRequest *req) {
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      return req->gfsk->sample_rate;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      // bpsk/dpsk/sdpsk share the same settings message (union aliasing), so req->bpsk works for all 3
      return req->bpsk->sample_rate;
    default:
      return 0;
  }
}

static uint32_t modem_get_bandwidth(ModemRequest *req) {
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      return req->gfsk->bandwidth;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      return (uint32_t) ((1 + req->bpsk->rrc_beta) * req->bpsk->baud_rate);
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

static int modem_create_bpsk_family(BpskModemSettings *req, uint64_t sample_rate, bpsk_modem_type type, uint32_t max_input_buffer_length, const char *debug_constellation_file, bpsk_modem **modem) {
  bpsk_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = req->baud_rate;
  settings.rrc_beta = req->rrc_beta;
  settings.rrc_delay = req->rrc_delay;
  settings.costas_bandwidth = req->costas_bandwidth;
  settings.symsync_filter_bank_size = req->symsync_filter_bank_size;
  settings.type = type;
  return bpsk_modem_create(&settings, max_input_buffer_length, debug_constellation_file, modem);
}

int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, const char *debug_freq_offset_file, const char *debug_constellation_file, const char *debug_baseband_file, sdr_modem **modem) {
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
  uint64_t sample_rate = modem_get_sample_rate(req);
  uint32_t bandwidth = modem_get_bandwidth(req);
  uint64_t decimated_sample_rate = sample_rate;
  uint32_t decimated_buffer_length = config->buffer_size;
  code = modem_halfband_decim_create(sample_rate, bandwidth, config->buffer_size, &result->halfband, &decimated_sample_rate, &decimated_buffer_length);
  if (code != 0) {
    modem_destroy(result);
    return code;
  }
  if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_GFSK) {
    code = gfsk_modem_create(req->gfsk, decimated_sample_rate, decimated_buffer_length, (gfsk_modem **) &result->modem);
    if (code != 0) {
      modem_destroy(result);
      return code;
    }
    result->modulate = gfsk_modem_modulate;
    result->demodulate = gfsk_modem_demodulate;
    result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
    result->destroy = gfsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_BPSK) {
    code = modem_create_bpsk_family(req->bpsk, decimated_sample_rate, NORMAL, decimated_buffer_length, debug_constellation_file, (bpsk_modem **) &result->modem);
    if (code != 0) {
      modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_DPSK) {
    code = modem_create_bpsk_family(req->dpsk, decimated_sample_rate, DIFFERENTIAL, decimated_buffer_length, debug_constellation_file, (bpsk_modem **) &result->modem);
    if (code != 0) {
      modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
  } else if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_SDPSK) {
    code = modem_create_bpsk_family(req->sdpsk, decimated_sample_rate, SYMMETRIC_DIFFERENTIAL, decimated_buffer_length, debug_constellation_file, (bpsk_modem **) &result->modem);
    if (code != 0) {
      modem_destroy(result);
      return code;
    }
    result->modulate = bpsk_modem_modulate;
    result->demodulate = bpsk_modem_demodulate;
    result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length;
    result->destroy = bpsk_modem_destroy;
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
      modem_destroy(result);
      return code;
    }
  }

  if (debug_freq_offset_file != NULL) {
    result->debug_freq_offset_file = fopen(debug_freq_offset_file, "wb");
    if (result->debug_freq_offset_file == NULL) {
      fprintf(stderr, "<3>unable to open debug freq offset file: %s\n", debug_freq_offset_file);
      modem_destroy(result);
      return -1;
    }
  }

  if (debug_baseband_file != NULL) {
    result->debug_baseband_file = fopen(debug_baseband_file, "wb");
    if (result->debug_baseband_file == NULL) {
      fprintf(stderr, "<3>unable to open debug baseband file: %s\n", debug_baseband_file);
      modem_destroy(result);
      return -1;
    }
  }

  *modem = result;
  return 0;
}

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
  if (modem->debug_freq_offset_file != NULL && *output != NULL) {
    fwrite(*output, sizeof(float complex), *output_len, modem->debug_freq_offset_file);
  }
  if (modem->freq_offset != NULL && *output != NULL) {
    freq_offset_process(*output, *output_len, output, output_len, modem->freq_offset);
  }
}

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem) {
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

size_t modem_max_modulation_buffer_length(sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  return modem->max_modulation_buffer_length(modem->modem);
}

void modem_destroy(sdr_modem *modem) {
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
