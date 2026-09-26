#include "gfsk_modem2.h"
#include "gfsk_correlator.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>

// internal samples per symbol of both chains. cpfskmod requires an even number, and the correlator
// template is built at this rate, so it is fixed: input at any other rate is resampled
#define GFSK_MODEM2_SPS 2
// filter delay in symbols of both tx gaussian pulse and rx matched filter
#define GFSK_MODEM2_FILTER_DELAY 3
#define GFSK_MODEM2_RESAMPLER_STOPBAND_ATTENUATION_DB 60.0f
#define GFSK_MODEM2_RESAMPLER_OUTPUT_MARGIN 16
#define GFSK_MODEM2_LOWPASS_NUM_TAPS 129
#define GFSK_MODEM2_LOWPASS_STOPBAND_ATTENUATION_DB 60.0f

// FIXME: hardcoded until it is moved to the settings. on-air symbols (i.e. after nrzi, msb first) of
// the ais training sequence (24 bits) + hdlc start flag. nrzi makes the polarity arbitrary, so
// both the sync word and its inverse are detected
// static const uint8_t GFSK_MODEM2_SYNC_WORD[] = {0xCC, 0xCC, 0xCC, 0xFE};
// static const uint8_t GFSK_MODEM2_SYNC_WORD[] = { 0x50, 0x72, 0xF6, 0x4B };
static const uint8_t GFSK_MODEM2_SYNC_WORD[] = {0xAA, 0xAA, 0xAA, 0x7E};
#define GFSK_MODEM2_SYNC_WORD_BITS 32
// normalized correlation (pearson, so independent of amplitude and frequency offset) to declare
// the sync word found
#define GFSK_MODEM2_CORRELATION_THRESHOLD 0.7f

struct gfsk_modem2_t {
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

  // gmsk receive filter on the discriminator output. dc gain is 1
  firfilt_rrrf matched_filter;
  float *matched_filter_output;

  // finds the sync word and samples the burst with the timing, dc and gain estimated from it
  gfsk_correlator *correlator;

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
static int gfsk_modem2_create_rx_filter(float bt, float **filter, unsigned int *filter_len) {
  unsigned int len = 2 * GFSK_MODEM2_SPS * GFSK_MODEM2_FILTER_DELAY + 1;
  float *result = malloc(sizeof(float) * len);
  if (result == NULL) {
    return -ENOMEM;
  }
  liquid_firdes_prototype(LIQUID_FIRFILT_GMSKRX, GFSK_MODEM2_SPS, GFSK_MODEM2_FILTER_DELAY, bt, 0.0f, result);
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

int gfsk_modem2_create(GfskModemSettings *settings, uint64_t sample_rate, uint32_t max_input_buffer_length, gfsk_modem2 **modem) {
  if (settings->baud_rate == 0 || settings->deviation == 0) {
    fprintf(stderr, "<3>gfsk modem2: baud_rate and deviation must not be 0\n");
    return -EINVAL;
  }
  if (settings->bt <= 0.0f || settings->bt > 1.0f) {
    fprintf(stderr, "<3>gfsk modem2: bt must be in (0, 1]: %f\n", settings->bt);
    return -EINVAL;
  }
  if ((double) sample_rate / (double) settings->baud_rate < 2.0 || (double) settings->sample_rate / (double) settings->baud_rate < 2.0) {
    fprintf(stderr, "<3>gfsk modem2: samples per symbol must be at least 2; check sample_rate/baud_rate\n");
    return -EINVAL;
  }
  uint64_t internal_sample_rate = (uint64_t) GFSK_MODEM2_SPS * (uint64_t) settings->baud_rate;
  bool rx_needs_resampling = sample_rate != internal_sample_rate;
  bool tx_needs_resampling = settings->sample_rate != internal_sample_rate;

  struct gfsk_modem2_t *result = malloc(sizeof(struct gfsk_modem2_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct gfsk_modem2_t){0};
  result->max_input_buffer_length = max_input_buffer_length;
  // phase change per symbol is pi * h; it is +-pi * deviation / (baud_rate / 2)
  result->modulation_index = (float) (2.0 * (double) settings->deviation / (double) settings->baud_rate);

  //////// RX chain

  size_t rx_max_input_buffer_length = max_input_buffer_length;
  if (rx_needs_resampling) {
    double resample_rate = (double) internal_sample_rate / (double) sample_rate;
    result->resampler_rx = msresamp_crcf_create((float) resample_rate, GFSK_MODEM2_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_rx == NULL) {
      gfsk_modem2_destroy(result);
      return -EINVAL;
    }
    result->resampler_rx_output_len = (size_t) ceil((double) max_input_buffer_length * resample_rate) + GFSK_MODEM2_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_rx_output = malloc(sizeof(float complex) * result->resampler_rx_output_len);
    if (result->resampler_rx_output == NULL) {
      gfsk_modem2_destroy(result);
      return -ENOMEM;
    }
    rx_max_input_buffer_length = result->resampler_rx_output_len;
  }

  if (settings->bandwidth != 0) {
    // bandwidth is the full occupied bandwidth, so the cutoff is half of it
    float lowpass_fc = (float) settings->bandwidth / 2.0f / (float) internal_sample_rate;
    if (lowpass_fc <= 0.0f || lowpass_fc >= 0.5f) {
      fprintf(stderr, "<3>gfsk modem2: bandwidth %uHz is not valid at internal sample rate %llu\n", settings->bandwidth, (unsigned long long) internal_sample_rate);
      gfsk_modem2_destroy(result);
      return -EINVAL;
    }
    result->lowpass = firfilt_crcf_create_kaiser(GFSK_MODEM2_LOWPASS_NUM_TAPS, lowpass_fc, GFSK_MODEM2_LOWPASS_STOPBAND_ATTENUATION_DB, 0.0f);
    if (result->lowpass == NULL) {
      gfsk_modem2_destroy(result);
      return -EINVAL;
    }
    result->lowpass_output = malloc(sizeof(float complex) * rx_max_input_buffer_length);
    if (result->lowpass_output == NULL) {
      gfsk_modem2_destroy(result);
      return -ENOMEM;
    }
  }

  // deviation / rx sample rate: the discriminator output is +-1 at +-deviation
  result->discriminator = freqdem_create((float) ((double) settings->deviation / (double) internal_sample_rate));
  if (result->discriminator == NULL) {
    gfsk_modem2_destroy(result);
    return -EINVAL;
  }
  result->discriminator_output = malloc(sizeof(float) * rx_max_input_buffer_length);
  if (result->discriminator_output == NULL) {
    gfsk_modem2_destroy(result);
    return -ENOMEM;
  }

  float *rx_filter = NULL;
  unsigned int rx_filter_len = 0;
  int code = gfsk_modem2_create_rx_filter(settings->bt, &rx_filter, &rx_filter_len);
  if (code != 0) {
    gfsk_modem2_destroy(result);
    return code;
  }
  result->matched_filter = firfilt_rrrf_create(rx_filter, rx_filter_len);
  code = gfsk_correlator_create(GFSK_MODEM2_SPS, settings->bt, GFSK_MODEM2_FILTER_DELAY, rx_filter, rx_filter_len, GFSK_MODEM2_SYNC_WORD, GFSK_MODEM2_SYNC_WORD_BITS, GFSK_MODEM2_CORRELATION_THRESHOLD, rx_max_input_buffer_length, &result->correlator);
  free(rx_filter);
  if (result->matched_filter == NULL) {
    gfsk_modem2_destroy(result);
    return -EINVAL;
  }
  if (code != 0) {
    gfsk_modem2_destroy(result);
    return code;
  }
  result->matched_filter_output = malloc(sizeof(float) * rx_max_input_buffer_length);
  if (result->matched_filter_output == NULL) {
    gfsk_modem2_destroy(result);
    return -ENOMEM;
  }

  //////// TX chain

  result->max_modulation_input_bits = (size_t) max_input_buffer_length * 8;
  result->max_modulation_buffer_length = result->max_modulation_input_bits * GFSK_MODEM2_SPS;
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    gfsk_modem2_destroy(result);
    return -ENOMEM;
  }

  result->mod = cpfskmod_create(1, result->modulation_index, GFSK_MODEM2_SPS, GFSK_MODEM2_FILTER_DELAY, settings->bt, LIQUID_CPFSK_GMSK);
  if (result->mod == NULL) {
    gfsk_modem2_destroy(result);
    return -EINVAL;
  }

  if (tx_needs_resampling) {
    double resample_rate = (double) settings->sample_rate / (double) internal_sample_rate;
    result->resampler_tx = msresamp_crcf_create((float) resample_rate, GFSK_MODEM2_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_tx == NULL) {
      gfsk_modem2_destroy(result);
      return -EINVAL;
    }
    result->resampler_tx_output_len = (size_t) ceil((double) result->max_modulation_buffer_length * resample_rate) + GFSK_MODEM2_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_tx_output = malloc(sizeof(float complex) * result->resampler_tx_output_len);
    if (result->resampler_tx_output == NULL) {
      gfsk_modem2_destroy(result);
      return -ENOMEM;
    }
  }

  *modem = result;
  return 0;
}

size_t gfsk_modem2_max_modulation_buffer_length(void *modem) {
  gfsk_modem2 *gfsk = (gfsk_modem2 *) modem;
  return gfsk->resampler_tx != NULL ? gfsk->resampler_tx_output_len : gfsk->max_modulation_buffer_length;
}

void gfsk_modem2_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem) {
  gfsk_modem2 *demod = (gfsk_modem2 *) modem;
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
  firfilt_rrrf_execute_block(demod->matched_filter, demod->discriminator_output, (unsigned int) input_len, demod->matched_filter_output);
  gfsk_correlator_process(demod->matched_filter_output, input_len, output, output_len, demod->correlator);
}

void gfsk_modem2_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem) {
  gfsk_modem2 *mod = (gfsk_modem2 *) modem;
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
      sample_index += GFSK_MODEM2_SPS;
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

void gfsk_modem2_destroy(void *modem) {
  if (modem == NULL) {
    return;
  }
  gfsk_modem2 *m = (gfsk_modem2 *) modem;
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
