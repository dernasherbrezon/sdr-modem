#include "gfsk_modem2.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>

// internal samples per symbol of both chains. cpfskmod requires an even number, and symsync's loop
// gain depends on it, so it is fixed: input at any other rate is resampled
#define GFSK_MODEM2_SPS 8
// filter delay in symbols of both tx gaussian pulse and rx matched filter
#define GFSK_MODEM2_FILTER_DELAY 3
// rolloff of the gmsk receive filter is bt * scale, clamped to 1. liquid's cpfskdem uses 0.8, but
// on the ber simulation (bt 0.5, h 1) 0.8 loses ~1.5dB against 2.0: bigger is better until the
// clamp. only tuned for that configuration
#ifndef GFSK_MODEM2_RX_BT_SCALE
#define GFSK_MODEM2_RX_BT_SCALE 2.0f
#endif
#define GFSK_MODEM2_SYMSYNC_FILTER_BANK_SIZE 32
#define GFSK_MODEM2_SYMSYNC_LOOP_BANDWIDTH 0.01f
#define GFSK_MODEM2_RESAMPLER_STOPBAND_ATTENUATION_DB 60.0f
#define GFSK_MODEM2_RESAMPLER_OUTPUT_MARGIN 16
#define GFSK_MODEM2_LOWPASS_NUM_TAPS 129
#define GFSK_MODEM2_LOWPASS_STOPBAND_ATTENUATION_DB 60.0f
// dc blocker time constant, in symbols. same as the window used by gfsk_modem.
// it runs at the internal sample rate, ahead of symsync: a carrier offset biases the timing error
// detector, and with the offset at half of the deviation the timing loop fails when the dc is only
// removed after it
#define GFSK_MODEM2_DC_BLOCK_SYMBOLS 32

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

  // gmsk receive matched filter + symbol timing recovery, output is 1 sample per symbol
  symsync_rrrf symbol_sync;
  size_t symsync_output_len;

  // optional. removes carrier frequency offset, which shows up as dc in the discriminator output
  iirfilt_rrrf dc_blocker;
  // soft symbols, +-1 for +-deviation
  float *soft;
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

// same as symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_GMSKRX, ...), but the prototype is scaled so
// that a constant input comes out unchanged: the input is the discriminator output (+-1), and the
// matched filter output has to stay +-1 too, so it can be used as the soft symbol.
// symsync divides its output by the samples per symbol, hence the extra factor. the timing error
// detector is not affected by the scale of the prototype
static symsync_rrrf gfsk_modem2_create_symsync(float bt) {
  unsigned int filter_len = 2 * GFSK_MODEM2_SPS * GFSK_MODEM2_FILTER_DELAY * GFSK_MODEM2_SYMSYNC_FILTER_BANK_SIZE + 1;
  float *filter = malloc(sizeof(float) * filter_len);
  if (filter == NULL) {
    return NULL;
  }
  liquid_firdes_prototype(LIQUID_FIRFILT_GMSKRX, GFSK_MODEM2_SPS * GFSK_MODEM2_SYMSYNC_FILTER_BANK_SIZE, GFSK_MODEM2_FILTER_DELAY, fminf(1.0f, GFSK_MODEM2_RX_BT_SCALE * bt), 0.0f, filter);
  float sum = 0.0f;
  for (unsigned int i = 0; i < filter_len; i++) {
    sum += filter[i];
  }
  // every polyphase branch gets 1/BANK_SIZE of the total
  float scale = (float) (GFSK_MODEM2_SYMSYNC_FILTER_BANK_SIZE * GFSK_MODEM2_SPS) / sum;
  for (unsigned int i = 0; i < filter_len; i++) {
    filter[i] *= scale;
  }
  symsync_rrrf result = symsync_rrrf_create(GFSK_MODEM2_SPS, GFSK_MODEM2_SYMSYNC_FILTER_BANK_SIZE, filter, filter_len);
  free(filter);
  return result;
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

  result->symbol_sync = gfsk_modem2_create_symsync(settings->bt);
  if (result->symbol_sync == NULL) {
    gfsk_modem2_destroy(result);
    return -EINVAL;
  }
  symsync_rrrf_set_output_rate(result->symbol_sync, 1);
  symsync_rrrf_set_lf_bw(result->symbol_sync, GFSK_MODEM2_SYMSYNC_LOOP_BANDWIDTH);
  result->symsync_output_len = rx_max_input_buffer_length;

  if (settings->use_dc_block) {
    result->dc_blocker = iirfilt_rrrf_create_dc_blocker(1.0f / (GFSK_MODEM2_DC_BLOCK_SYMBOLS * GFSK_MODEM2_SPS));
    if (result->dc_blocker == NULL) {
      gfsk_modem2_destroy(result);
      return -EINVAL;
    }
  }

  result->soft = malloc(sizeof(float) * result->symsync_output_len);
  if (result->soft == NULL) {
    gfsk_modem2_destroy(result);
    return -ENOMEM;
  }

  result->output = malloc(sizeof(int8_t) * result->symsync_output_len);
  if (result->output == NULL) {
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

  if (demod->dc_blocker != NULL) {
    iirfilt_rrrf_execute_block(demod->dc_blocker, demod->discriminator_output, (unsigned int) input_len, demod->discriminator_output);
  }

  unsigned int num_symbols = 0;
  symsync_rrrf_execute(demod->symbol_sync, demod->discriminator_output, (unsigned int) input_len, demod->soft, &num_symbols);

  for (unsigned int i = 0; i < num_symbols; i++) {
    float r = demod->soft[i] * 127.0f;
    if (r > INT8_MAX) {
      r = INT8_MAX;
    } else if (r < INT8_MIN) {
      r = INT8_MIN;
    }
    demod->output[i] = (int8_t) rintf(r);
  }

  *output = demod->output;
  *output_len = num_symbols;
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
  if (m->symbol_sync != NULL) {
    symsync_rrrf_destroy(m->symbol_sync);
  }
  if (m->dc_blocker != NULL) {
    iirfilt_rrrf_destroy(m->dc_blocker);
  }
  if (m->soft != NULL) {
    free(m->soft);
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
