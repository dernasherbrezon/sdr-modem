#include "gmsk_modem.h"
#include "quadrature_demod.h"
#include "clock_recovery_mm.h"
#include "dc_blocker.h"
#include <math.h>
#include <volk/volk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "interp_fir_filter.h"
#include "frequency_modulator.h"
#include "gaussian_taps.h"
#include "lpf.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct gmsk_modem_t {
  lpf *lpf1;
  lpf *lpf2;
  quadrature_demod *quad_demod;
  dc_blocker *dc;
  clock_mm *clock;

  int8_t *output;
  size_t output_len;

  interp_fir_filter *filter;
  frequency_modulator *freq_mod;

  float *temp_input;
  size_t temp_input_len;
};

int gmsk_modem_create(GmskModemSettings *req, uint32_t max_input_buffer_length, gmsk_modem **demod) {
  struct gmsk_modem_t *result = malloc(sizeof(struct gmsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct gmsk_modem_t){0};

  double carson_cutoff = (double) llabs(req->deviation) + (double) req->baud_rate / 2;
  int code = lpf_create(1, req->sample_rate, (uint64_t) carson_cutoff, (uint32_t) (0.1f * carson_cutoff), max_input_buffer_length, sizeof(float complex), &result->lpf1);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }
  code = quadrature_demod_create((float) ((double) req->sample_rate / (2 * M_PI * (double) req->deviation)), max_input_buffer_length, &result->quad_demod);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }
  code = lpf_create(req->decimation, req->sample_rate, req->baud_rate / 2, req->transition_width, max_input_buffer_length, sizeof(float), &result->lpf2);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  float sps = (float) ((double) req->sample_rate / req->baud_rate / req->decimation);

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

  size_t gaussian_taps_len = 4 * sps;
  float *gaussian_taps = NULL;
  code = gaussian_taps_create(1.0F, sps, req->bt, gaussian_taps_len, &gaussian_taps);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }
  size_t square_wave_len = (int) sps;
  float *square_wave = malloc(sizeof(float) * square_wave_len);
  if (square_wave == NULL) {
    free(gaussian_taps);
    gmsk_modem_destroy(result);
    return -ENOMEM;
  }
  for (size_t i = 0; i < square_wave_len; i++) {
    square_wave[i] = 1.0F;
  }

  float *taps = NULL;
  size_t taps_len = 0;
  code = gmsk_modem_convolve(gaussian_taps, gaussian_taps_len, square_wave, square_wave_len, &taps, &taps_len);
  free(gaussian_taps);
  free(square_wave);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  code = interp_fir_filter_create(taps, taps_len, (int) sps, result->temp_input_len, &result->filter);
  if (code != 0) {
    free(taps);
    gmsk_modem_destroy(result);
    return code;
  }

  float sensitivity = (float) (2 * M_PI * (double) req->deviation / (double) req->sample_rate);
  code = frequency_modulator_create(sensitivity, (int) sps * result->temp_input_len, &result->freq_mod);
  if (code != 0) {
    gmsk_modem_destroy(result);
    return code;
  }

  *demod = result;
  return 0;
}

void gmsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem) {
  gmsk_modem *demod = (gmsk_modem *) modem;
  float complex *lpf_output = NULL;
  size_t lpf_output_len = 0;
  lpf_process(input, input_len, (void **) &lpf_output, &lpf_output_len, demod->lpf1);

  float *qd_output = NULL;
  size_t qd_output_len = 0;
  quadrature_demod_process(lpf_output, lpf_output_len, &qd_output, &qd_output_len, demod->quad_demod);

  float *lpf2_output = NULL;
  size_t lpf2_output_len = 0;
  lpf_process(qd_output, qd_output_len, (void **) &lpf2_output, &lpf2_output_len, demod->lpf2);

  float *dc_output = NULL;
  size_t dc_output_len = 0;
  if (demod->dc != NULL) {
    dc_blocker_process(lpf2_output, lpf2_output_len, &dc_output, &dc_output_len, demod->dc);
  } else {
    dc_output = lpf2_output;
    dc_output_len = lpf2_output_len;
  }

  float *clock_output = NULL;
  size_t clock_output_len = 0;
  clock_mm_process(dc_output, dc_output_len, &clock_output, &clock_output_len, demod->clock);

  volk_32f_s32f_convert_8i(demod->output, clock_output, 127.0f, clock_output_len);

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
  if (demod->lpf1 != NULL) {
    lpf_destroy(demod->lpf1);
  }
  if (demod->quad_demod != NULL) {
    quadrature_demod_destroy(demod->quad_demod);
  }
  if (demod->lpf2 != NULL) {
    lpf_destroy(demod->lpf2);
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
