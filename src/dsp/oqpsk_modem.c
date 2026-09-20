#include "oqpsk_modem.h"
#include "mmse_fir_interpolator.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// stopband attenuation for the arbitrary-rate resamplers used when sample_rate is not
// an exact multiple of baud_rate. 60dB is liquid-dsp's typical default for msresamp_crcf.
#define OQPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB 60.0f
// extra headroom (in samples) added on top of the theoretical ceil(len * rate) output size,
// since msresamp_crcf's actual output count for a given input block can vary slightly
#define OQPSK_MODEM_RESAMPLER_OUTPUT_MARGIN 16

// Mueller & Mueller timing recovery for OQPSK's staggered I/Q, built directly on top of the same
// mmse_fir_interpolator primitive clock_recovery_mm.c uses for its single-rail (BPSK/GFSK) case.
// A plain pair of independent clock_mm instances -- one per rail -- was tried first and rejected:
// each rail's own Mueller & Mueller loop locks onto its own eye just fine (confirmed against a
// known-good, fixed-offset decimation), but the two loops' fractional-sample timing estimates
// jitter independently of one another even on a noiseless signal, since decision-directed timing
// recovery is not perfectly stable by nature. Pairing the two rails' recovered symbols back into
// one complex sample for costas carrier correction then feeds that inter-rail jitter into the
// phase detector as if it were carrier rotation, which destabilizes the loop regardless of how
// small costas_bandwidth is. The fix is to never let the two rails drift independently in the
// first place: a single Mueller & Mueller loop tracks I's timing (I decides the timing error, the
// same way clock_mm does for a single real rail), and Q is sampled off the SAME (ii, mu) shifted
// by exactly half_sps -- the fixed relationship the TX side built in -- so the two rails can never
// disagree about timing.
struct oqpsk_symbol_clock_t {
  mmse_fir_interpolator *interp;
  unsigned int half_sps;

  float omega;
  float omega_mid;
  float omega_lim;
  float gain_omega;
  float mu;
  float gain_mu;
  float last_i;

  float *i_working;
  float *q_working;
  size_t history_offset;
  size_t working_len_total;

  float *i_output;
  float *q_output;
  size_t output_len;
};

static float oqpsk_symbol_clock_slice(float x) {
  return x < 0 ? -1.0f : 1.0f;
}

static inline float oqpsk_symbol_clock_branchless_clip(float x, float clip) {
  return 0.5f * (fabsf(x + clip) - fabsf(x - clip));
}

static void oqpsk_symbol_clock_destroy(struct oqpsk_symbol_clock_t *clock) {
  if (clock == NULL) {
    return;
  }
  if (clock->interp != NULL) {
    mmse_fir_interpolator_destroy(clock->interp);
  }
  if (clock->i_working != NULL) {
    free(clock->i_working);
  }
  if (clock->q_working != NULL) {
    free(clock->q_working);
  }
  if (clock->i_output != NULL) {
    free(clock->i_output);
  }
  if (clock->q_output != NULL) {
    free(clock->q_output);
  }
  free(clock);
}

static int oqpsk_symbol_clock_create(float omega, float gain_omega, float mu, float gain_mu, unsigned int half_sps, size_t output_len, struct oqpsk_symbol_clock_t **clock) {
  struct oqpsk_symbol_clock_t *result = malloc(sizeof(struct oqpsk_symbol_clock_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct oqpsk_symbol_clock_t){0};
  result->half_sps = half_sps;
  result->mu = mu;
  result->omega = omega;
  result->gain_omega = gain_omega;
  result->gain_mu = gain_mu;
  result->omega_mid = omega;
  // same 1% relative frequency search range clock_recovery_mm.c uses
  result->omega_lim = omega * 0.01f;

  int code = mmse_fir_interpolator_create(&result->interp);
  if (code != 0) {
    oqpsk_symbol_clock_destroy(result);
    return code;
  }

  result->output_len = output_len;
  result->i_output = malloc(sizeof(float) * output_len);
  result->q_output = malloc(sizeof(float) * output_len);
  if (result->i_output == NULL || result->q_output == NULL) {
    oqpsk_symbol_clock_destroy(result);
    return -ENOMEM;
  }

  unsigned int taps_len = (unsigned int) mmse_fir_interpolator_taps(result->interp);
  // Q is read half_sps samples ahead of I, so its window needs that much extra lookahead on top
  // of the interpolator's own taps
  result->working_len_total = output_len + taps_len + half_sps;
  result->i_working = malloc(sizeof(float) * result->working_len_total);
  result->q_working = malloc(sizeof(float) * result->working_len_total);
  if (result->i_working == NULL || result->q_working == NULL) {
    oqpsk_symbol_clock_destroy(result);
    return -ENOMEM;
  }
  memset(result->i_working, 0, sizeof(float) * result->working_len_total);
  memset(result->q_working, 0, sizeof(float) * result->working_len_total);

  *clock = result;
  return 0;
}

static void oqpsk_symbol_clock_process(const float *i_in, const float *q_in, size_t input_len, float **i_out, float **q_out, size_t *out_len, struct oqpsk_symbol_clock_t *clock) {
  if (input_len > clock->output_len) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, clock->output_len);
    *i_out = NULL;
    *q_out = NULL;
    *out_len = 0;
    return;
  }
  memcpy(clock->i_working + clock->history_offset, i_in, input_len * sizeof(float));
  memcpy(clock->q_working + clock->history_offset, q_in, input_len * sizeof(float));

  unsigned int taps_len = (unsigned int) mmse_fir_interpolator_taps(clock->interp);
  size_t needed = (size_t) taps_len + clock->half_sps;
  int ii = 0;
  int oo = 0;
  int previous = 0;
  size_t working_len = clock->history_offset + input_len;
  if (working_len < needed) {
    clock->history_offset = working_len;
    *i_out = NULL;
    *q_out = NULL;
    *out_len = 0;
    return;
  }
  size_t max_index = working_len - (needed - 1);
  float mm_val;

  while ((size_t) ii < max_index && (size_t) oo < clock->output_len) {
    clock->i_output[oo] = mmse_fir_interpolator_process(clock->i_working + ii, clock->mu, clock->interp);
    clock->q_output[oo] = mmse_fir_interpolator_process(clock->q_working + ii + clock->half_sps, clock->mu, clock->interp);
    if (isnan(clock->i_output[oo]) || isnan(clock->q_output[oo])) {
      clock->i_output[oo] = 0.0f;
      clock->q_output[oo] = 0.0f;
      previous = ii;
      ii += (int) floorf(clock->omega);
      oo++;
      continue;
    }

    // timing error is derived from the I rail alone -- same Mueller & Mueller slicer-product
    // detector clock_recovery_mm.c uses -- and both I and Q are decimated together off it, so the
    // two rails can never disagree about timing (see the struct comment above)
    mm_val = oqpsk_symbol_clock_slice(clock->last_i) * clock->i_output[oo] - oqpsk_symbol_clock_slice(clock->i_output[oo]) * clock->last_i;
    clock->last_i = clock->i_output[oo];
    previous = ii;

    clock->omega = clock->omega + clock->gain_omega * mm_val;
    clock->omega = clock->omega_mid + oqpsk_symbol_clock_branchless_clip(clock->omega - clock->omega_mid, clock->omega_lim);
    clock->mu = clock->mu + clock->omega + clock->gain_mu * mm_val;
    ii += (int) floorf(clock->mu);
    clock->mu = clock->mu - floorf(clock->mu);
    oo++;
  }

  size_t last_index;
  if ((size_t) ii > working_len) {
    last_index = previous;
  } else {
    last_index = ii;
  }
  clock->history_offset = working_len - last_index;
  memmove(clock->i_working, clock->i_working + last_index, sizeof(float) * clock->history_offset);
  memmove(clock->q_working, clock->q_working + last_index, sizeof(float) * clock->history_offset);

  *i_out = clock->i_output;
  *q_out = clock->q_output;
  *out_len = (size_t) oo;
}

struct oqpsk_modem_t {
  uint64_t sample_rate;
  uint32_t baud_rate;
  unsigned int samples_per_symbol;
  unsigned int half_sps;
  size_t max_input_buffer_length;

  // present only when sample_rate is not an exact multiple of baud_rate: bridges the actual
  // I/Q sample rate and the nearest internal rate (samples_per_symbol * baud_rate) that the
  // rest of the pipeline below (matched filter/firinterp) requires to be an integer multiple
  bool needs_resampling;
  msresamp_crcf resampler_rx;
  float complex *resampler_rx_output;
  size_t resampler_rx_output_len;
  msresamp_crcf resampler_tx;
  float complex *resampler_tx_output;
  size_t resampler_tx_output_len;

  // automatic gain control: normalizes input signal amplitude ahead of the matched filter,
  // since timing/carrier recovery below both assume a roughly constant envelope
  agc_crcf rx_agc;
  float complex *agc_output;
  size_t agc_output_len;

  // matched RRC filter, realized from an RRC prototype internally by liquid-dsp. unlike
  // bpsk_modem, this runs at full sample rate with no decimation: timing recovery below does its
  // own decimation, jointly, for both I and Q (see oqpsk_symbol_clock_t)
  firfilt_crcf matched_filter;
  float complex *matched_output;
  size_t matched_output_len;
  float *i_rail;
  float *q_rail;
  size_t rail_len;

  // joint I/Q Mueller & Mueller timing recovery -- see oqpsk_symbol_clock_t above
  struct oqpsk_symbol_clock_t *symbol_clock;

  // carrier recovery: tracks and corrects residual frequency/phase offset once per recovered
  // symbol, the same way bpsk_modem.c's costas loop does. costas_mag_ema is a slow envelope
  // tracker (independent of rx_agc) used only to gate the loop: a symbol far below the recent
  // average magnitude is still ramping up through the matched filter/timing recovery pipeline
  // (e.g. the first few symbols after RX starts, or after a signal dropout) rather than a settled
  // decision, and letting a handful of those into the phase detector can knock a low-bandwidth
  // loop far enough off frequency that it takes a very long time to recover
  nco_crcf costas;
  float costas_mag_ema;
  bool costas_mag_ema_initialized;
  float complex *corrected_symbols;
  size_t corrected_symbols_len;

  int8_t *bit_output;
  size_t output_len;

  // TX chain: I and Q bit streams are pulse-shaped independently, then combined with the Q rail
  // delayed by half a symbol period relative to I
  firinterp_crcf interp_i;
  firinterp_crcf interp_q;
  float complex *i_shaped;
  float complex *q_shaped;
  // carries the still-undelivered tail (last half_sps samples) of the previous call's shaped Q
  // rail, so the half-symbol delay stays continuous across modulate() calls
  float complex *q_delay_line;
  size_t max_modulation_input_bits;
  size_t max_modulation_buffer_length;
  float complex *modulation_output;

  FILE *debug_constellation_file;
  float complex *debug_constellation;
  size_t debug_constellation_len;
};

int oqpsk_modem_create(const oqpsk_modem_settings *settings, uint32_t max_input_buffer_length, oqpsk_modem **modem) {
  if (settings->baud_rate == 0) {
    fprintf(stderr, "<3>oqpsk modem: baud_rate must not be 0\n");
    return -EINVAL;
  }
  bool needs_resampling = (settings->sample_rate % settings->baud_rate) != 0;
  unsigned int sps;
  if (needs_resampling) {
    sps = (unsigned int) llround((double) settings->sample_rate / (double) settings->baud_rate);
  } else {
    sps = (unsigned int) (settings->sample_rate / settings->baud_rate);
  }
  if (sps < 2) {
    fprintf(stderr, "<3>oqpsk modem: samples per symbol (%u) must be at least 2\n", sps);
    return -EINVAL;
  }
  // internal working rate the rest of the pipeline (matched filter/firinterp) operates at: the
  // nearest exact multiple of baud_rate to the requested sample_rate
  uint64_t internal_sample_rate = (uint64_t) sps * (uint64_t) settings->baud_rate;
  double resample_rate_rx = needs_resampling ? (double) internal_sample_rate / (double) settings->sample_rate : 1.0;
  double resample_rate_tx = needs_resampling ? (double) settings->sample_rate / (double) internal_sample_rate : 1.0;

  struct oqpsk_modem_t *result = malloc(sizeof(struct oqpsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct oqpsk_modem_t){0};
  result->sample_rate = settings->sample_rate;
  result->baud_rate = settings->baud_rate;
  result->samples_per_symbol = sps;
  result->half_sps = sps / 2;
  result->max_input_buffer_length = max_input_buffer_length;
  result->needs_resampling = needs_resampling;

  if (needs_resampling) {
    result->resampler_rx = msresamp_crcf_create((float) resample_rate_rx, OQPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    result->resampler_tx = msresamp_crcf_create((float) resample_rate_tx, OQPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_rx == NULL || result->resampler_tx == NULL) {
      oqpsk_modem_destroy(result);
      return -EINVAL;
    }
    result->resampler_rx_output_len = (size_t) ceil((double) max_input_buffer_length * resample_rate_rx) + OQPSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_rx_output = malloc(sizeof(float complex) * result->resampler_rx_output_len);
    if (result->resampler_rx_output == NULL) {
      oqpsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  // capacity (in samples) the rest of the RX chain below needs to size its buffers to: when
  // resampling, that's the resampled buffer size, not the raw input one
  size_t rx_capacity = needs_resampling ? result->resampler_rx_output_len : max_input_buffer_length;

  result->agc_output_len = rx_capacity;
  result->agc_output = malloc(sizeof(float complex) * result->agc_output_len);
  if (result->agc_output == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }
  result->rx_agc = agc_crcf_create();
  if (result->rx_agc == NULL) {
    oqpsk_modem_destroy(result);
    return -EINVAL;
  }

  result->matched_filter = firfilt_crcf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, 0.0f);
  if (result->matched_filter == NULL) {
    oqpsk_modem_destroy(result);
    return -EINVAL;
  }
  result->matched_output_len = rx_capacity;
  result->matched_output = malloc(sizeof(float complex) * result->matched_output_len);
  if (result->matched_output == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }
  result->rail_len = rx_capacity;
  result->i_rail = malloc(sizeof(float) * result->rail_len);
  result->q_rail = malloc(sizeof(float) * result->rail_len);
  if (result->i_rail == NULL || result->q_rail == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }

  float sps_f = (float) sps;
  int code = oqpsk_symbol_clock_create(sps_f, (sps_f * (float) M_PI) / 100, 0.5f, 0.5f / 8.0f, result->half_sps, rx_capacity, &result->symbol_clock);
  if (code != 0) {
    oqpsk_modem_destroy(result);
    return code;
  }

  result->costas = nco_crcf_create(LIQUID_NCO);
  if (result->costas == NULL) {
    oqpsk_modem_destroy(result);
    return -EINVAL;
  }
  nco_crcf_pll_set_bandwidth(result->costas, settings->costas_bandwidth);
  // at most one recovered symbol per samples_per_symbol input samples; rx_capacity is a safe
  // (generous) upper bound on the number of symbols the symbol clock can produce from one call
  result->corrected_symbols_len = rx_capacity;
  result->corrected_symbols = malloc(sizeof(float complex) * result->corrected_symbols_len);
  if (result->corrected_symbols == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }

  // one (I, Q) bit pair per recovered symbol
  result->output_len = 2 * rx_capacity;
  result->bit_output = malloc(sizeof(int8_t) * result->output_len);
  if (result->bit_output == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }

  result->interp_i = firinterp_crcf_create_prototype(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, 0.0f);
  result->interp_q = firinterp_crcf_create_prototype(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, 0.0f);
  if (result->interp_i == NULL || result->interp_q == NULL) {
    oqpsk_modem_destroy(result);
    return -EINVAL;
  }
  result->i_shaped = malloc(sizeof(float complex) * sps);
  result->q_shaped = malloc(sizeof(float complex) * sps);
  if (result->i_shaped == NULL || result->q_shaped == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }
  result->q_delay_line = malloc(sizeof(float complex) * result->half_sps);
  if (result->q_delay_line == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }
  memset(result->q_delay_line, 0, sizeof(float complex) * result->half_sps);

  // 2 bits (I + Q) per symbol, samples_per_symbol samples per symbol
  result->max_modulation_input_bits = (size_t) max_input_buffer_length * 8;
  result->max_modulation_buffer_length = (result->max_modulation_input_bits / 2) * sps;
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    oqpsk_modem_destroy(result);
    return -ENOMEM;
  }

  if (needs_resampling) {
    result->resampler_tx_output_len = (size_t) ceil((double) result->max_modulation_buffer_length * resample_rate_tx) + OQPSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_tx_output = malloc(sizeof(float complex) * result->resampler_tx_output_len);
    if (result->resampler_tx_output == NULL) {
      oqpsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  *modem = result;
  return 0;
}

int oqpsk_modem_set_debug_constellation_file(const char *debug_constellation_file, oqpsk_modem *modem) {
  // replace whatever was configured before
  if (modem->debug_constellation_file != NULL) {
    fclose(modem->debug_constellation_file);
    modem->debug_constellation_file = NULL;
  }
  if (modem->debug_constellation != NULL) {
    free(modem->debug_constellation);
    modem->debug_constellation = NULL;
  }
  modem->debug_constellation_len = 0;
  if (debug_constellation_file == NULL) {
    return 0;
  }

  // same capacity as the rest of the RX chain (see rx_capacity in oqpsk_modem_create)
  size_t len = modem->corrected_symbols_len;
  float complex *buffer = malloc(sizeof(float complex) * len);
  if (buffer == NULL) {
    return -ENOMEM;
  }
  FILE *file = fopen(debug_constellation_file, "wb");
  if (file == NULL) {
    fprintf(stderr, "<3>unable to open debug constellation file: %s\n", debug_constellation_file);
    free(buffer);
    return -1;
  }
  modem->debug_constellation = buffer;
  modem->debug_constellation_len = len;
  modem->debug_constellation_file = file;
  return 0;
}

size_t oqpsk_modem_max_modulation_buffer_length(void *modem_v) {
  oqpsk_modem *modem = modem_v;
  return modem->needs_resampling ? modem->resampler_tx_output_len : modem->max_modulation_buffer_length;
}

void oqpsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *demod_v) {
  oqpsk_modem *demod = demod_v;
  if (input_len > demod->max_input_buffer_length) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, demod->max_input_buffer_length);
    *output = NULL;
    *output_len = 0;
    return;
  }

  const float complex *agc_input = input;
  unsigned int agc_input_len = (unsigned int) input_len;
  if (demod->needs_resampling) {
    unsigned int resampled_len = 0;
    msresamp_crcf_execute(demod->resampler_rx, (float complex *) input, (unsigned int) input_len, demod->resampler_rx_output, &resampled_len);
    agc_input = demod->resampler_rx_output;
    agc_input_len = resampled_len;
  }

  agc_crcf_execute_block(demod->rx_agc, (float complex *) agc_input, agc_input_len, demod->agc_output);

  firfilt_crcf_execute_block(demod->matched_filter, demod->agc_output, agc_input_len, demod->matched_output);

  for (unsigned int i = 0; i < agc_input_len; i++) {
    demod->i_rail[i] = crealf(demod->matched_output[i]);
    demod->q_rail[i] = cimagf(demod->matched_output[i]);
  }

  // decimate I and Q to the symbol rate jointly (see oqpsk_symbol_clock_t): symbol index k of
  // symbols_i and symbols_q both trace back to the same original (I, Q) dibit, sampled exactly
  // half_sps samples apart, so from here on they form one recovered complex symbol per k
  float *symbols_i = NULL;
  float *symbols_q = NULL;
  size_t num_symbols = 0;
  oqpsk_symbol_clock_process(demod->i_rail, demod->q_rail, agc_input_len, &symbols_i, &symbols_q, &num_symbols, demod->symbol_clock);

  for (size_t i = 0; i < num_symbols; i++) {
    float complex mixed;
    nco_crcf_mix_down(demod->costas, symbols_i[i] + symbols_q[i] * I, &mixed);
    demod->corrected_symbols[i] = mixed;

    float magnitude = cabsf(mixed);
    if (!demod->costas_mag_ema_initialized) {
      demod->costas_mag_ema = magnitude;
      demod->costas_mag_ema_initialized = true;
    }
    // gate the loop on symbols that are already near the recent average amplitude, before
    // updating the average itself -- see the costas_mag_ema field comment for why
    if (magnitude > 0.3f * demod->costas_mag_ema) {
      // 4th-power (non-decision-directed) costas phase detector: a QPSK/OQPSK constellation has
      // 4 points spaced 90 degrees apart, so raising to the 4th power is always a multiple of 360
      // degrees regardless of which point the data lands on, cancelling the data modulation the
      // same way bpsk_modem.c's SYMMETRIC_DIFFERENTIAL detector does for its own 4-point
      // constellation. the symbol is normalized to unit magnitude first so the detector carries
      // phase information only, not amplitude.
      // OQPSK data sits on the diagonals (45/135/225/315 degrees), where the 4th power is -1 and
      // sin(4*theta) falls with theta, so the error has to be negated for the loop to be stable
      // there. without the negation the loop is stable on the axes instead: locked 45 degrees off,
      // where the sign of each rail is essentially random
      float complex normalized = magnitude > 1e-6f ? mixed / magnitude : mixed;
      float complex squared = normalized * normalized;
      float complex fourth = squared * squared;
      float phase_error = -cimagf(fourth);
      nco_crcf_pll_step(demod->costas, phase_error);
    }
    nco_crcf_step(demod->costas);
    demod->costas_mag_ema += 0.05f * (magnitude - demod->costas_mag_ema);

    float soft_i = crealf(mixed) * 127.0f;
    if (soft_i > INT8_MAX) {
      soft_i = INT8_MAX;
    } else if (soft_i < INT8_MIN) {
      soft_i = INT8_MIN;
    }
    float soft_q = cimagf(mixed) * 127.0f;
    if (soft_q > INT8_MAX) {
      soft_q = INT8_MAX;
    } else if (soft_q < INT8_MIN) {
      soft_q = INT8_MIN;
    }
    demod->bit_output[2 * i] = (int8_t) rintf(soft_i);
    demod->bit_output[2 * i + 1] = (int8_t) rintf(soft_q);
  }

  if (demod->debug_constellation_file != NULL && demod->debug_constellation != NULL) {
    memcpy(demod->debug_constellation, demod->corrected_symbols, sizeof(float complex) * num_symbols);
    fwrite(demod->debug_constellation, sizeof(float complex), num_symbols, demod->debug_constellation_file);
  }

  *output = demod->bit_output;
  *output_len = 2 * num_symbols;
}

void oqpsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *mod_v) {
  oqpsk_modem *mod = mod_v;
  if (input_len * 8 > mod->max_modulation_input_bits) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, mod->max_modulation_input_bits / 8);
    *output = NULL;
    *output_len = 0;
    return;
  }

  size_t sps = mod->samples_per_symbol;
  size_t half_sps = mod->half_sps;
  size_t sample_index = 0;
  for (size_t i = 0; i < input_len; i++) {
    // 2 bits per symbol: bit 2*s -> I, bit 2*s+1 -> Q, MSB first, matching the interleaving
    // oqpsk_modem_demodulate() produces
    for (int s = 0; s < 4; s++) {
      unsigned int i_bit = (input[i] >> (7 - 2 * s)) & 1U;
      unsigned int q_bit = (input[i] >> (6 - 2 * s)) & 1U;
      float complex i_symbol = i_bit ? (1.0f + 0.0f * I) : (-1.0f + 0.0f * I);
      float complex q_symbol = q_bit ? (1.0f + 0.0f * I) : (-1.0f + 0.0f * I);

      if (mod->debug_constellation_file != NULL) {
        float complex sym = crealf(i_symbol) + crealf(q_symbol) * I;
        fwrite(&sym, sizeof(float complex), 1, mod->debug_constellation_file);
      }

      firinterp_crcf_execute(mod->interp_i, i_symbol, mod->i_shaped);
      firinterp_crcf_execute(mod->interp_q, q_symbol, mod->q_shaped);

      // combine I and (half-symbol-delayed) Q into the complex baseband output. the delay is
      // what turns plain QPSK into OQPSK: it keeps I and Q from ever transitioning at the same
      // instant, which is what avoids 180 degree phase jumps through the origin
      for (size_t k = 0; k < half_sps; k++) {
        mod->modulation_output[sample_index + k] = crealf(mod->i_shaped[k]) + crealf(mod->q_delay_line[k]) * I;
      }
      for (size_t k = half_sps; k < sps; k++) {
        mod->modulation_output[sample_index + k] = crealf(mod->i_shaped[k]) + crealf(mod->q_shaped[k - half_sps]) * I;
      }
      for (size_t k = 0; k < half_sps; k++) {
        mod->q_delay_line[k] = mod->q_shaped[sps - half_sps + k];
      }

      sample_index += sps;
    }
  }

  if (mod->needs_resampling) {
    unsigned int resampled_len = 0;
    msresamp_crcf_execute(mod->resampler_tx, mod->modulation_output, (unsigned int) sample_index, mod->resampler_tx_output, &resampled_len);
    *output = mod->resampler_tx_output;
    *output_len = resampled_len;
  } else {
    *output = mod->modulation_output;
    *output_len = sample_index;
  }
}

void oqpsk_modem_destroy(void *modem_v) {
  oqpsk_modem *modem = modem_v;
  if (modem == NULL) {
    return;
  }
  if (modem->resampler_rx != NULL) {
    msresamp_crcf_destroy(modem->resampler_rx);
  }
  if (modem->resampler_tx != NULL) {
    msresamp_crcf_destroy(modem->resampler_tx);
  }
  if (modem->resampler_rx_output != NULL) {
    free(modem->resampler_rx_output);
  }
  if (modem->resampler_tx_output != NULL) {
    free(modem->resampler_tx_output);
  }
  if (modem->rx_agc != NULL) {
    agc_crcf_destroy(modem->rx_agc);
  }
  if (modem->agc_output != NULL) {
    free(modem->agc_output);
  }
  if (modem->matched_filter != NULL) {
    firfilt_crcf_destroy(modem->matched_filter);
  }
  if (modem->matched_output != NULL) {
    free(modem->matched_output);
  }
  if (modem->i_rail != NULL) {
    free(modem->i_rail);
  }
  if (modem->q_rail != NULL) {
    free(modem->q_rail);
  }
  if (modem->symbol_clock != NULL) {
    oqpsk_symbol_clock_destroy(modem->symbol_clock);
  }
  if (modem->costas != NULL) {
    nco_crcf_destroy(modem->costas);
  }
  if (modem->corrected_symbols != NULL) {
    free(modem->corrected_symbols);
  }
  if (modem->bit_output != NULL) {
    free(modem->bit_output);
  }
  if (modem->interp_i != NULL) {
    firinterp_crcf_destroy(modem->interp_i);
  }
  if (modem->interp_q != NULL) {
    firinterp_crcf_destroy(modem->interp_q);
  }
  if (modem->i_shaped != NULL) {
    free(modem->i_shaped);
  }
  if (modem->q_shaped != NULL) {
    free(modem->q_shaped);
  }
  if (modem->q_delay_line != NULL) {
    free(modem->q_delay_line);
  }
  if (modem->modulation_output != NULL) {
    free(modem->modulation_output);
  }
  if (modem->debug_constellation_file != NULL) {
    fclose(modem->debug_constellation_file);
  }
  if (modem->debug_constellation != NULL) {
    free(modem->debug_constellation);
  }
  free(modem);
}
