#include "gmsk_modem.h"
#include "quadrature_demod.h"
#include "clock_recovery_mm.h"
#include "dc_blocker.h"
#include "halfband_decim.h"
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "interp_fir_filter.h"
#include "frequency_modulator.h"
#include "gaussian_taps.h"
#include "fir_filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// hardcoded per design: half-band decimator stop-band attenuation
#define GMSK_MODEM_STOPBAND_ATTENUATION_DB 60.0f
// half-band decimator: keep the decimated rate at least this many times the signal bandwidth
// so that the half-band filter's own transition band does not clip the signal
#define GMSK_MODEM_HALFBAND_MIN_OVERSAMPLE 2.0f
#define GMSK_MODEM_HALFBAND_MAX_STAGES 6
// matched filter: keep at least this many samples per symbol so clock recovery has room to lock
#define GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER 2.0f

struct gmsk_modem_t {
  // NULL when the sample rate is already close enough to the required bandwidth
  halfband_decim *halfband;
  quadrature_demod *quad_demod;
  // fine gaussian matched filter, decimates the discriminator output down to just above
  // GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER samples per symbol
  fir_filter *matched_filter;
  dc_blocker *dc;
  clock_mm *clock;

  int8_t *output;
  size_t output_len;

  interp_fir_filter *filter;
  frequency_modulator *freq_mod;

  float *temp_input;
  size_t temp_input_len;
};

static unsigned int gmsk_modem_estimate_halfband_stages(uint64_t sample_rate, uint32_t bandwidth) {
  if (bandwidth == 0) {
    return 0;
  }
  uint64_t required_rate = (uint64_t) ceilf(GMSK_MODEM_HALFBAND_MIN_OVERSAMPLE * (float) bandwidth);
  unsigned int num_stages = 0;
  while (num_stages < GMSK_MODEM_HALFBAND_MAX_STAGES && (sample_rate >> (num_stages + 1)) >= required_rate) {
    num_stages++;
  }
  return num_stages;
}

// choose the decimation factor for the matched filter so that the resulting samples per symbol
// stay above GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER.
static unsigned int gmsk_modem_estimate_decimation(float sps) {
  unsigned int decimation = (unsigned int) (sps / GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER);
  while (decimation > 1 && (sps / (float) decimation) <= GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER) {
    decimation--;
  }
  if (decimation < 1) {
    decimation = 1;
  }
  return decimation;
}

int gmsk_modem_create(GmskModemSettings *req, uint32_t max_input_buffer_length, gmsk_modem **demod) {
  struct gmsk_modem_t *result = malloc(sizeof(struct gmsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct gmsk_modem_t){0};

  unsigned int halfband_stages = gmsk_modem_estimate_halfband_stages(req->sample_rate, req->bandwidth);
  uint64_t sample_rate = req->sample_rate;
  int code;
  if (halfband_stages > 0) {
    sample_rate = req->sample_rate >> halfband_stages;
    float cutoff = ((float) req->bandwidth / 2.0f) / (float) sample_rate;
    // liquid advise avoid 0 and 0.5, so put some guards here
    if (cutoff < 0.05f) {
      cutoff = 0.05f;
    } else if (cutoff > 0.45f) {
      cutoff = 0.45f;
    }
    code = halfband_decim_create(halfband_stages, cutoff, GMSK_MODEM_STOPBAND_ATTENUATION_DB, max_input_buffer_length, &result->halfband);
    if (code != 0) {
      gmsk_modem_destroy(result);
      return code;
    }
    max_input_buffer_length = max_input_buffer_length / (1u << halfband_stages) + 1;
  }

  code = quadrature_demod_create((float) ((double) sample_rate / (2 * M_PI * (double) req->deviation)), max_input_buffer_length, &result->quad_demod);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  float sps = (float) ((double) sample_rate / (double) req->baud_rate);
  unsigned int matched_decimation = gmsk_modem_estimate_decimation(sps);
  // strictly below the minimum the discriminator output can't be demodulated at all. exactly at
  // the minimum is still accepted, since that's also the sps used for TX pulse shaping.
  if (sps / (float) matched_decimation < GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER) {
    fprintf(stderr, "<3>gmsk modem: samples per symbol after matched filter (%f) is below the minimum required %.1f; check sample_rate/baud_rate/bandwidth\n", sps, GMSK_MODEM_MIN_SPS_AFTER_MATCHED_FILTER);
    gmsk_modem_destroy(result);
    return -EINVAL;
  }

  size_t rx_gaussian_taps_len = (size_t) (4 * sps);
  if (rx_gaussian_taps_len < (size_t) matched_decimation * 2 + 1) {
    rx_gaussian_taps_len = (size_t) matched_decimation * 2 + 1;
  }
  float *rx_gaussian_taps = NULL;
  code = gaussian_taps_create(sps, req->bt, rx_gaussian_taps_len, &rx_gaussian_taps);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }
  code = fir_filter_create((uint8_t) matched_decimation, rx_gaussian_taps, rx_gaussian_taps_len, max_input_buffer_length, sizeof(float), &result->matched_filter);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  if (req->use_dc_block) {
    code = dc_blocker_create((int) ceilf(sps * 32), &result->dc);
    if (code != 0) {
      gmsk_modem_destroy(result);
      return code;
    }
  }

  code = clock_mm_create(sps, (sps * (float) M_PI) / 100, 0.5f, 0.5f / 8.0f, 0.01f, max_input_buffer_length, &result->clock);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  result->output_len = max_input_buffer_length;
  result->output = malloc(sizeof(int8_t) * result->output_len);
  if (result->output == NULL) {
    gmsk_modem_destroy(result);
    return -ENOMEM;
  }

  result->temp_input_len = max_input_buffer_length * 8;
  result->temp_input = malloc(sizeof(float) * result->temp_input_len);
  if (result->temp_input == NULL) {
    gmsk_modem_destroy(result);
    return -ENOMEM;
  }

  float tx_sps = (float) ((double) req->sample_rate / req->baud_rate);
  size_t tx_gaussian_taps_len = (size_t) (4 * tx_sps);
  float *tx_gaussian_taps = NULL;
  code = gaussian_taps_create(tx_sps, req->bt, tx_gaussian_taps_len, &tx_gaussian_taps);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }
  size_t square_wave_len = (int) tx_sps;
  float *square_wave = malloc(sizeof(float) * square_wave_len);
  if (square_wave == NULL) {
    free(tx_gaussian_taps);
    gmsk_modem_destroy(result);
    return -ENOMEM;
  }
  for (size_t i = 0; i < square_wave_len; i++) {
    square_wave[i] = 1.0F;
  }

  float *taps = NULL;
  size_t taps_len = 0;
  code = gmsk_modem_convolve(tx_gaussian_taps, tx_gaussian_taps_len, square_wave, square_wave_len, &taps, &taps_len);
  free(tx_gaussian_taps);
  free(square_wave);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  code = interp_fir_filter_create(taps, taps_len, (int) tx_sps, result->temp_input_len, &result->filter);
  if (code != 0) {
    free(taps);
    gmsk_modem_destroy(result);
    return code;
  }

  float sensitivity = (float) (2 * M_PI * (double) req->deviation / (double) req->sample_rate);
  code = frequency_modulator_create(sensitivity, (int) tx_sps * result->temp_input_len, &result->freq_mod);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  *demod = result;
  return 0;
}

void gmsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem) {
  gmsk_modem *demod = (gmsk_modem *) modem;

  const float complex *quad_input = input;
  size_t quad_input_len = input_len;
  if (demod->halfband != NULL) {
    float complex *halfband_output = NULL;
    size_t halfband_output_len = 0;
    halfband_decim_process(input, input_len, &halfband_output, &halfband_output_len, demod->halfband);
    quad_input = halfband_output;
    quad_input_len = halfband_output_len;
  }

  float *qd_output = NULL;
  size_t qd_output_len = 0;
  quadrature_demod_process((float complex *) quad_input, quad_input_len, &qd_output, &qd_output_len, demod->quad_demod);

  float *matched_output = NULL;
  size_t matched_output_len = 0;
  fir_filter_process(qd_output, qd_output_len, (void **) &matched_output, &matched_output_len, demod->matched_filter);

  float *dc_output = NULL;
  size_t dc_output_len = 0;
  if (demod->dc != NULL) {
    dc_blocker_process(matched_output, matched_output_len, &dc_output, &dc_output_len, demod->dc);
  } else {
    dc_output = matched_output;
    dc_output_len = matched_output_len;
  }

  float *clock_output = NULL;
  size_t clock_output_len = 0;
  clock_mm_process(dc_output, dc_output_len, &clock_output, &clock_output_len, demod->clock);

  for (size_t i = 0; i < clock_output_len; i++) {
    float r = clock_output[i] * 127.0f;
    if (r > INT8_MAX) {
      r = INT8_MAX;
    } else if (r < INT8_MIN) {
      r = INT8_MIN;
    }
    demod->output[i] = (int8_t) rintf(r);
  }

  *output = demod->output;
  *output_len = clock_output_len;
}

void gmsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem) {
  struct gmsk_modem_t *mod = modem;
  if (input_len > mod->temp_input_len / 8) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, mod->temp_input_len / 8);
    *output = NULL;
    *output_len = 0;
    return;
  }
  size_t temp_index = 0;
  for (size_t i = 0; i < input_len; i++) {
    for (int j = 0; j < 8; j++) {
      int bit = (input[i] >> (7 - j)) & 1;
      if (bit == 0) {
        mod->temp_input[temp_index] = -1.0F;
      } else {
        mod->temp_input[temp_index] = 1.0F;
      }
      temp_index++;
    }
  }

  float *filtered = NULL;
  size_t filtered_len = 0;
  interp_fir_filter_process(mod->temp_input, temp_index, &filtered, &filtered_len, mod->filter);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  frequency_modulator_process(filtered, filtered_len, &modulated, &modulated_len, mod->freq_mod);

  *output = modulated;
  *output_len = modulated_len;
}

int gmsk_modem_convolve(float *x, size_t x_len, float *y, size_t y_len, float **out, size_t *out_len) {
  size_t result_len = x_len + y_len - 1;
  float *result = malloc(sizeof(float) * result_len);
  if (result == NULL) {
    return -ENOMEM;
  }
  float *temp = malloc(sizeof(float) * result_len);
  if (temp == NULL) {
    return -ENOMEM;
  }
  memset(temp, 0, sizeof(float) * result_len);
  memcpy(temp, x, sizeof(float) * x_len);
  for (size_t i = 0; i < result_len; i++) {
    float sum = 0.0F;
    for (int j = 0, k = i; j < y_len && k >= 0; j++, k--) {
      sum += y[j] * temp[k];
    }
    result[i] = sum;
  }
  free(temp);

  *out = result;
  *out_len = result_len;
  return 0;
}

void gmsk_modem_destroy(void *modem) {
  if (modem == NULL) {
    return;
  }
  gmsk_modem *demod = (gmsk_modem *) modem;
  if (demod->halfband != NULL) {
    halfband_decim_destroy(demod->halfband);
  }
  if (demod->quad_demod != NULL) {
    quadrature_demod_destroy(demod->quad_demod);
  }
  if (demod->matched_filter != NULL) {
    fir_filter_destroy(demod->matched_filter);
  }
  if (demod->dc != NULL) {
    dc_blocker_destroy(demod->dc);
  }
  if (demod->clock != NULL) {
    clock_mm_destroy(demod->clock);
  }
  if (demod->output != NULL) {
    free(demod->output);
  }
  if (demod->temp_input != NULL) {
    free(demod->temp_input);
  }
  if (demod->filter != NULL) {
    interp_fir_filter_destroy(demod->filter);
  }
  if (demod->freq_mod != NULL) {
    frequency_modulator_destroy(demod->freq_mod);
  }
  free(demod);
}
