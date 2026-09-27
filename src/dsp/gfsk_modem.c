#include "gfsk_modem.h"
#include "gfsk_correlator.h"
#include "dc_blocker.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>

// internal samples per symbol of both chains. cpfskmod requires an even number, and the correlator
// template is built at this rate, so it is fixed: input at any other rate is resampled
#define GFSK_MODEM_SPS 2
// filter delay in symbols of both tx gaussian pulse and rx matched filter
#define GFSK_MODEM_FILTER_DELAY 3
#define GFSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB 60.0f
#define GFSK_MODEM_RESAMPLER_OUTPUT_MARGIN 16
#define GFSK_MODEM_LOWPASS_NUM_TAPS 129
#define GFSK_MODEM_LOWPASS_STOPBAND_ATTENUATION_DB 60.0f

// normalized correlation (pearson, so independent of amplitude and frequency offset) to declare
// the sync word found
#define GFSK_MODEM_CORRELATION_THRESHOLD 0.7f

// without sync word: symbol timing recovery. number of polyphase filters and the loop bandwidth.
// the bandwidth is a trade-off between pulling in the timing within the preamble of a burst and
// jitter from noise
#define GFSK_MODEM_SYMSYNC_FILTER_BANK_SIZE 32
#define GFSK_MODEM_SYMSYNC_LOOP_BANDWIDTH 0.05f
// dc blocker length in symbols
#define GFSK_MODEM_DC_BLOCK_SYMBOLS 32

struct gfsk_modem_t {
  float modulation_index;

  // RX chain ////////////////////////////////
  size_t max_input_buffer_length;

  // present only when the rx sample rate is not an even multiple of baud_rate
  msresamp_crcf resampler_rx;
  float complex *resampler_rx_output;
  size_t resampler_rx_output_len;

  // channel filter ahead of the discriminator, present only when settings->bandwidth is set.
  // the resampler passes noise up to the internal nyquist (4 * baud_rate), way wider than the
  // signal, and the discriminator is non-linear: it turns wideband noise into much more output
  // noise than a matched filter behind it can remove. the filter has to be before the discriminator
  firfilt_crcf lowpass;
  float complex *lowpass_output;

  // frequency discriminator: +-1 for +-deviation
  freqdem discriminator;
  float *discriminator_output;

  // with sync word ///////////////////////////
  // gmsk receive filter on the discriminator output. dc gain is 1
  firfilt_rrrf matched_filter;
  float *matched_filter_output;

  // finds the sync word and samples the burst with the timing, dc and gain estimated from it
  gfsk_correlator *correlator;

  // without sync word ////////////////////////
  // removes the frequency offset, present only when settings->use_dc_block is set
  dc_blocker *dc;

  // gmsk receive filter (polyphase, dc gain 1) and symbol timing recovery. its timing error
  // detector assumes h = 0.5
  symsync_rrrf symbol_sync;
  float *symbol_sync_output;
  int8_t *output;

  // TX chain ////////////////////////////////
  size_t max_modulation_input_bits;
  size_t max_modulation_buffer_length;
  float complex *modulation_output;

  cpfskmod mod;

  msresamp_crcf resampler_tx;
  float complex *resampler_tx_output;
  size_t resampler_tx_output_len;
};

// gmsk rx prototype, bt must be the same as the one of the tx pulse (cpfskmod, LIQUID_CPFSK_GMSK):
// gmskrx inverts that pulse, so that tx * rx has no isi. scaled to dc gain 1, so that the output
// stays +-1 for +-deviation
static int gfsk_modem_create_rx_filter(float bt, float **filter, unsigned int *filter_len) {
  unsigned int len = 2 * GFSK_MODEM_SPS * GFSK_MODEM_FILTER_DELAY + 1;
  float *result = malloc(sizeof(float) * len);
  if (result == NULL) {
    return -ENOMEM;
  }
  liquid_firdes_prototype(LIQUID_FIRFILT_GMSKRX, GFSK_MODEM_SPS, GFSK_MODEM_FILTER_DELAY, bt, 0.0f, result);
  float sum = 0.0f;
  for (unsigned int i = 0; i < len; i++) {
    sum += result[i];
  }
  for (unsigned int i = 0; i < len; i++) {
    result[i] /= sum;
  }
  *filter = result;
  *filter_len = len;
  return 0;
}

// same gmsk rx prototype as gfsk_modem_create_rx_filter, designed at GFSK_MODEM_SPS * filter bank
// size and split into the polyphase filter bank. symsync divides its output by the samples per
// symbol, so every sub-filter is scaled to dc gain GFSK_MODEM_SPS to get dc gain 1 overall
static symsync_rrrf gfsk_modem_create_symbol_sync(float bt) {
  unsigned int k = GFSK_MODEM_SPS * GFSK_MODEM_SYMSYNC_FILTER_BANK_SIZE;
  unsigned int len = 2 * k * GFSK_MODEM_FILTER_DELAY + 1;
  float *filter = malloc(sizeof(float) * len);
  if (filter == NULL) {
    return NULL;
  }
  liquid_firdes_prototype(LIQUID_FIRFILT_GMSKRX, k, GFSK_MODEM_FILTER_DELAY, bt, 0.0f, filter);
  float sum = 0.0f;
  for (unsigned int i = 0; i < len; i++) {
    sum += filter[i];
  }
  for (unsigned int i = 0; i < len; i++) {
    filter[i] *= (float) (GFSK_MODEM_SYMSYNC_FILTER_BANK_SIZE * GFSK_MODEM_SPS) / sum;
  }
  symsync_rrrf result = symsync_rrrf_create(GFSK_MODEM_SPS, GFSK_MODEM_SYMSYNC_FILTER_BANK_SIZE, filter, len);
  free(filter);
  if (result == NULL) {
    return NULL;
  }
  symsync_rrrf_set_output_rate(result, 1);
  symsync_rrrf_set_lf_bw(result, GFSK_MODEM_SYMSYNC_LOOP_BANDWIDTH);
  return result;
}

static int gfsk_modem_create_symbol_timing(GfskModemSettings *settings, size_t max_input_buffer_length, struct gfsk_modem_t *result) {
  if (settings->use_dc_block) {
    int code = dc_blocker_create(GFSK_MODEM_SPS * GFSK_MODEM_DC_BLOCK_SYMBOLS, &result->dc);
    if (code != 0) {
      return code;
    }
  }
  result->symbol_sync = gfsk_modem_create_symbol_sync(settings->bt);
  if (result->symbol_sync == NULL) {
    return -EINVAL;
  }
  // 1 symbol per GFSK_MODEM_SPS samples, plus a few when the timing loop slips
  size_t max_symbols = max_input_buffer_length / GFSK_MODEM_SPS + 4;
  result->symbol_sync_output = malloc(sizeof(float) * max_symbols);
  if (result->symbol_sync_output == NULL) {
    return -ENOMEM;
  }
  result->output = malloc(sizeof(int8_t) * max_symbols);
  if (result->output == NULL) {
    return -ENOMEM;
  }
  return 0;
}

static int gfsk_modem_create_correlator(GfskModemSettings *settings, uint64_t syncword, uint32_t syncword_bits, size_t max_input_buffer_length, struct gfsk_modem_t *result) {
  float *rx_filter = NULL;
  unsigned int rx_filter_len = 0;
  int code = gfsk_modem_create_rx_filter(settings->bt, &rx_filter, &rx_filter_len);
  if (code != 0) {
    return code;
  }
  result->matched_filter = firfilt_rrrf_create(rx_filter, rx_filter_len);
  code = gfsk_correlator_create(GFSK_MODEM_SPS, settings->bt, GFSK_MODEM_FILTER_DELAY, rx_filter, rx_filter_len, syncword, syncword_bits, GFSK_MODEM_CORRELATION_THRESHOLD, max_input_buffer_length, &result->correlator);
  free(rx_filter);
  if (result->matched_filter == NULL) {
    return -EINVAL;
  }
  if (code != 0) {
    return code;
  }
  result->matched_filter_output = malloc(sizeof(float) * max_input_buffer_length);
  if (result->matched_filter_output == NULL) {
    return -ENOMEM;
  }
  return 0;
}

int gfsk_modem_create(GfskModemSettings *settings, uint64_t syncword, uint32_t syncword_bits, uint64_t sample_rate, uint32_t max_input_buffer_length, gfsk_modem **modem) {
  if (settings->baud_rate == 0 || settings->deviation == 0) {
    fprintf(stderr, "<3>gfsk modem: baud_rate and deviation must not be 0\n");
    return -EINVAL;
  }
  if (settings->bt <= 0.0f || settings->bt > 1.0f) {
    fprintf(stderr, "<3>gfsk modem: bt must be in (0, 1]: %f\n", settings->bt);
    return -EINVAL;
  }
  if (syncword_bits > sizeof(syncword) * 8) {
    fprintf(stderr, "<3>gfsk modem: syncword_bits must not be more than %zu: %u\n", sizeof(syncword) * 8, syncword_bits);
    return -EINVAL;
  }
  if ((double) sample_rate / (double) settings->baud_rate < 2.0 || (double) settings->sample_rate / (double) settings->baud_rate < 2.0) {
    fprintf(stderr, "<3>gfsk modem: samples per symbol must be at least 2; check sample_rate/baud_rate\n");
    return -EINVAL;
  }
  uint64_t internal_sample_rate = (uint64_t) GFSK_MODEM_SPS * (uint64_t) settings->baud_rate;
  bool rx_needs_resampling = sample_rate != internal_sample_rate;
  bool tx_needs_resampling = settings->sample_rate != internal_sample_rate;

  struct gfsk_modem_t *result = malloc(sizeof(struct gfsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct gfsk_modem_t){0};
  result->max_input_buffer_length = max_input_buffer_length;
  // phase change per symbol is pi * h; it is +-pi * deviation / (baud_rate / 2)
  result->modulation_index = (float) (2.0 * (double) settings->deviation / (double) settings->baud_rate);

  //////// RX chain

  size_t rx_max_input_buffer_length = max_input_buffer_length;
  if (rx_needs_resampling) {
    double resample_rate = (double) internal_sample_rate / (double) sample_rate;
    result->resampler_rx = msresamp_crcf_create((float) resample_rate, GFSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_rx == NULL) {
      gfsk_modem_destroy(result);
      return -EINVAL;
    }
    result->resampler_rx_output_len = (size_t) ceil((double) max_input_buffer_length * resample_rate) + GFSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_rx_output = malloc(sizeof(float complex) * result->resampler_rx_output_len);
    if (result->resampler_rx_output == NULL) {
      gfsk_modem_destroy(result);
      return -ENOMEM;
    }
    rx_max_input_buffer_length = result->resampler_rx_output_len;
  }

  // bandwidth is the full occupied bandwidth, so the cutoff is half of it
  float lowpass_fc = (float) settings->bandwidth / 2.0f / (float) internal_sample_rate;
  // at or above the internal nyquist the filter would not limit anything: the input is already
  // band-limited to it (by the resampler, or by being sampled at the internal rate). it is the case
  // for h >= 1, where carson's bandwidth 2 * (deviation + baud_rate / 2) >= GFSK_MODEM_SPS * baud_rate
  if (settings->bandwidth != 0 && lowpass_fc < 0.5f) {
    result->lowpass = firfilt_crcf_create_kaiser(GFSK_MODEM_LOWPASS_NUM_TAPS, lowpass_fc, GFSK_MODEM_LOWPASS_STOPBAND_ATTENUATION_DB, 0.0f);
    if (result->lowpass == NULL) {
      gfsk_modem_destroy(result);
      return -EINVAL;
    }
    result->lowpass_output = malloc(sizeof(float complex) * rx_max_input_buffer_length);
    if (result->lowpass_output == NULL) {
      gfsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  // deviation / rx sample rate: the discriminator output is +-1 at +-deviation
  result->discriminator = freqdem_create((float) ((double) settings->deviation / (double) internal_sample_rate));
  if (result->discriminator == NULL) {
    gfsk_modem_destroy(result);
    return -EINVAL;
  }
  result->discriminator_output = malloc(sizeof(float) * rx_max_input_buffer_length);
  if (result->discriminator_output == NULL) {
    gfsk_modem_destroy(result);
    return -ENOMEM;
  }

  int code;
  if (syncword_bits != 0) {
    code = gfsk_modem_create_correlator(settings, syncword, syncword_bits, rx_max_input_buffer_length, result);
  } else {
    code = gfsk_modem_create_symbol_timing(settings, rx_max_input_buffer_length, result);
  }
  if (code != 0) {
    gfsk_modem_destroy(result);
    return code;
  }

  //////// TX chain

  result->max_modulation_input_bits = (size_t) max_input_buffer_length * 8;
  result->max_modulation_buffer_length = result->max_modulation_input_bits * GFSK_MODEM_SPS;
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    gfsk_modem_destroy(result);
    return -ENOMEM;
  }

  result->mod = cpfskmod_create(1, result->modulation_index, GFSK_MODEM_SPS, GFSK_MODEM_FILTER_DELAY, settings->bt, LIQUID_CPFSK_GMSK);
  if (result->mod == NULL) {
    gfsk_modem_destroy(result);
    return -EINVAL;
  }

  if (tx_needs_resampling) {
    double resample_rate = (double) settings->sample_rate / (double) internal_sample_rate;
    result->resampler_tx = msresamp_crcf_create((float) resample_rate, GFSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_tx == NULL) {
      gfsk_modem_destroy(result);
      return -EINVAL;
    }
    result->resampler_tx_output_len = (size_t) ceil((double) result->max_modulation_buffer_length * resample_rate) + GFSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_tx_output = malloc(sizeof(float complex) * result->resampler_tx_output_len);
    if (result->resampler_tx_output == NULL) {
      gfsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  *modem = result;
  return 0;
}

size_t gfsk_modem_max_modulation_buffer_length(void *modem) {
  gfsk_modem *gfsk = (gfsk_modem *) modem;
  return gfsk->resampler_tx != NULL ? gfsk->resampler_tx_output_len : gfsk->max_modulation_buffer_length;
}

void gfsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem) {
  gfsk_modem *demod = (gfsk_modem *) modem;
  if (input_len > demod->max_input_buffer_length) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, demod->max_input_buffer_length);
    *output = NULL;
    *output_len = 0;
    return;
  }

  if (demod->resampler_rx != NULL) {
    unsigned int resampled_len = 0;
    msresamp_crcf_execute(demod->resampler_rx, (float complex *) input, (unsigned int) input_len, demod->resampler_rx_output, &resampled_len);
    input = demod->resampler_rx_output;
    input_len = resampled_len;
  }

  if (demod->lowpass != NULL) {
    firfilt_crcf_execute_block(demod->lowpass, (float complex *) input, (unsigned int) input_len, demod->lowpass_output);
    input = demod->lowpass_output;
  }

  freqdem_demodulate_block(demod->discriminator, (float complex *) input, (unsigned int) input_len, demod->discriminator_output);
  if (demod->correlator != NULL) {
    firfilt_rrrf_execute_block(demod->matched_filter, demod->discriminator_output, (unsigned int) input_len, demod->matched_filter_output);
    gfsk_correlator_process(demod->matched_filter_output, input_len, output, output_len, demod->correlator);
    return;
  }

  float *samples = demod->discriminator_output;
  size_t samples_len = input_len;
  if (demod->dc != NULL) {
    dc_blocker_process(samples, samples_len, &samples, &samples_len, demod->dc);
  }
  // symsync's loop integrates the timing error into its resampling rate without any bound. between
  // bursts the input is noise, which drives the rate far off (by several % after a few seconds), and
  // the next burst is too short to pull it back. the rate is known exactly here (the input is resampled
  // to GFSK_MODEM_SPS), so it is reset to the nominal after every sample: only the timing phase is
  // tracked. set_output_rate is the only public api (liquid 1.4) that resets the rate. it does not
  // touch the phase: the timing error is applied right after it is computed, within the same sample
  unsigned int symbols_len = 0;
  for (size_t i = 0; i < samples_len; i++) {
    unsigned int current = 0;
    symsync_rrrf_execute(demod->symbol_sync, samples + i, 1, demod->symbol_sync_output + symbols_len, &current);
    symbols_len += current;
    symsync_rrrf_set_output_rate(demod->symbol_sync, 1);
  }
  // no sync word to estimate the amplitude from: +-1 is the configured deviation
  for (unsigned int i = 0; i < symbols_len; i++) {
    float r = demod->symbol_sync_output[i] * 127.0f;
    if (r > 127.0f) {
      r = 127.0f;
    } else if (r < -127.0f) {
      r = -127.0f;
    }
    demod->output[i] = (int8_t) rintf(r);
  }
  *output = demod->output;
  *output_len = symbols_len;
}

void gfsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem) {
  gfsk_modem *mod = (gfsk_modem *) modem;
  if (input_len * 8 > mod->max_modulation_input_bits) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, mod->max_modulation_input_bits / 8);
    *output = NULL;
    *output_len = 0;
    return;
  }

  size_t sample_index = 0;
  for (size_t i = 0; i < input_len; i++) {
    for (int j = 0; j < 8; j++) {
      unsigned int bit = (input[i] >> (7 - j)) & 1U;
      cpfskmod_modulate(mod->mod, bit, mod->modulation_output + sample_index);
      sample_index += GFSK_MODEM_SPS;
    }
  }

  *output = mod->modulation_output;
  *output_len = sample_index;

  if (mod->resampler_tx != NULL) {
    unsigned int resampled_len = 0;
    msresamp_crcf_execute(mod->resampler_tx, *output, (unsigned int) *output_len, mod->resampler_tx_output, &resampled_len);
    *output = mod->resampler_tx_output;
    *output_len = resampled_len;
  }
}

void gfsk_modem_destroy(void *modem) {
  if (modem == NULL) {
    return;
  }
  gfsk_modem *m = (gfsk_modem *) modem;
  if (m->resampler_rx != NULL) {
    msresamp_crcf_destroy(m->resampler_rx);
  }
  if (m->resampler_rx_output != NULL) {
    free(m->resampler_rx_output);
  }
  if (m->lowpass != NULL) {
    firfilt_crcf_destroy(m->lowpass);
  }
  if (m->lowpass_output != NULL) {
    free(m->lowpass_output);
  }
  if (m->discriminator != NULL) {
    freqdem_destroy(m->discriminator);
  }
  if (m->discriminator_output != NULL) {
    free(m->discriminator_output);
  }
  if (m->matched_filter != NULL) {
    firfilt_rrrf_destroy(m->matched_filter);
  }
  if (m->matched_filter_output != NULL) {
    free(m->matched_filter_output);
  }
  if (m->correlator != NULL) {
    gfsk_correlator_destroy(m->correlator);
  }
  if (m->dc != NULL) {
    dc_blocker_destroy(m->dc);
  }
  if (m->symbol_sync != NULL) {
    symsync_rrrf_destroy(m->symbol_sync);
  }
  if (m->symbol_sync_output != NULL) {
    free(m->symbol_sync_output);
  }
  if (m->output != NULL) {
    free(m->output);
  }
  if (m->mod != NULL) {
    cpfskmod_destroy(m->mod);
  }
  if (m->modulation_output != NULL) {
    free(m->modulation_output);
  }
  if (m->resampler_tx != NULL) {
    msresamp_crcf_destroy(m->resampler_tx);
  }
  if (m->resampler_tx_output != NULL) {
    free(m->resampler_tx_output);
  }
  free(m);
}
