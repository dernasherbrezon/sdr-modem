#include "bpsk_modem.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB 60.0f
#define BPSK_MODEM_TARGET_SPS 15
#define BPSK_MODEM_RESAMPLER_OUTPUT_MARGIN 16
#define BPSK_MODEM_LOWPASS_NUM_TAPS 129
#define BPSK_MODEM_LOWPASS_STOPBAND_ATTENUATION_DB 60.0f

struct bpsk_modem_t {
  unsigned int samples_per_symbol;
  size_t max_input_buffer_length;

  // RX chain ////////////////////////////////
  // present only when sample_rate is not an exact multiple of baud_rate: bridges the actual
  // I/Q sample rate and the nearest internal rate (samples_per_symbol * baud_rate) that the
  // rest of the pipeline below (symsync/firinterp) requires to be an integer multiple
  msresamp_crcf resampler_rx;
  float complex *resampler_rx_output;
  size_t resampler_rx_output_len;

  // optional low-pass filter ahead of rx_agc; NULL when settings->bandwidth is 0
  firfilt_crcf lowpass_filter;
  float complex *lowpass_output;
  size_t lowpass_output_len;

  // automatic gain control: normalizes input signal amplitude ahead of the symbol
  // synchronizer, since symsync's timing error detector assumes a roughly constant envelope
  agc_crcf rx_agc;
  float complex *agc_output;
  size_t agc_output_len;

  // matched RRC filter + symbol timing recovery (Mueller & Muller-style timing error detector),
  // realized from an RRC prototype internally by liquid-dsp
  symsync_crcf symbol_sync;
  float complex *symsync_output;
  size_t symsync_output_len;

  // carrier recovery: tracks and corrects small residual frequency/phase offsets left after
  // the symbol synchronizer, one correction per recovered symbol
  nco_crcf costas;

  psk_modem_type type;
  modemcf mod;

  int8_t *bit_output;
  size_t bit_output_len;

  // TX chain ////////////////////////////////
  size_t max_modulation_input_bits;
  size_t max_modulation_buffer_length;
  float complex *modulation_output;

  firinterp_crcf interp;

  msresamp_crcf resampler_tx;
  float complex *resampler_tx_output;
  size_t resampler_tx_output_len;

  // SYMMETRIC_DIFFERENTIAL only: liquid-dsp has no built-in modem for it, so bits are
  // encoded/decoded as +-90 degree rotations relative to the previous symbol instead of
  // going through modemcf. rx_prev_symbol carries the previously received (noisy) symbol
  // for symbol-by-symbol differential detection.
  float complex tx_prev_symbol;
  float complex rx_prev_symbol;

  FILE *debug_constellation_file;
  float complex *debug_constellation;
  size_t debug_constellation_len;
};

int bpsk_modem_create(const bpsk_modem_settings *settings, uint32_t max_input_buffer_length, bpsk_modem **modem) {
  if (settings->baud_rate == 0) {
    fprintf(stderr, "<3>bpsk modem: baud_rate must not be 0\n");
    return -EINVAL;
  }
  unsigned int input_sps = settings->sample_rate / settings->baud_rate;
  bool needs_resampling;
  unsigned int sps;
  if (input_sps > BPSK_MODEM_TARGET_SPS) {
    needs_resampling = true;
    sps = BPSK_MODEM_TARGET_SPS;
  } else if (settings->sample_rate % settings->baud_rate != 0) {
    needs_resampling = true;
    sps = input_sps;
  } else {
    needs_resampling = false;
    sps = input_sps;
  }
  if (sps < 2) {
    fprintf(stderr, "<3>bpsk modem: samples per symbol (%u) must be at least 2\n", sps);
    return -EINVAL;
  }

  uint64_t internal_sample_rate = (uint64_t) sps * (uint64_t) settings->baud_rate;


  struct bpsk_modem_t *result = malloc(sizeof(struct bpsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct bpsk_modem_t){0};
  result->samples_per_symbol = sps;
  result->max_input_buffer_length = max_input_buffer_length;
  result->type = settings->type;

  uint32_t rx_max_input_buffer_length = max_input_buffer_length;
  if (needs_resampling) {
    double resample_rate_rx = (double) internal_sample_rate / (double) settings->sample_rate;
    result->resampler_rx = msresamp_crcf_create((float) resample_rate_rx, BPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_rx == NULL) {
      bpsk_modem_destroy(result);
      return -EINVAL;
    }
    result->resampler_rx_output_len = (size_t) ceil((double) rx_max_input_buffer_length * resample_rate_rx) + BPSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_rx_output = malloc(sizeof(float complex) * result->resampler_rx_output_len);
    if (result->resampler_rx_output == NULL) {
      bpsk_modem_destroy(result);
      return -ENOMEM;
    }
    rx_max_input_buffer_length = result->resampler_rx_output_len;
  }

  if (settings->bandwidth != 0) {
    float lowpass_fc = (float) settings->bandwidth / (float) internal_sample_rate;
    if (lowpass_fc <= 0.0f || lowpass_fc >= 0.5f) {
      fprintf(stderr, "<3>bpsk modem: lowpass cutoff %uHz is not valid at internal sample rate %llu\n", settings->bandwidth, (unsigned long long) internal_sample_rate);
      bpsk_modem_destroy(result);
      return -EINVAL;
    }
    result->lowpass_filter = firfilt_crcf_create_kaiser(BPSK_MODEM_LOWPASS_NUM_TAPS, lowpass_fc, BPSK_MODEM_LOWPASS_STOPBAND_ATTENUATION_DB, 0.0f);
    if (result->lowpass_filter == NULL) {
      bpsk_modem_destroy(result);
      return -EINVAL;
    }
    result->lowpass_output_len = rx_max_input_buffer_length;
    result->lowpass_output = malloc(sizeof(float complex) * result->lowpass_output_len);
    if (result->lowpass_output == NULL) {
      bpsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  result->agc_output_len = rx_max_input_buffer_length;
  result->agc_output = malloc(sizeof(float complex) * result->agc_output_len);
  if (result->agc_output == NULL) {
    bpsk_modem_destroy(result);
    return -ENOMEM;
  }
  result->rx_agc = agc_crcf_create();
  if (result->rx_agc == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }

  result->symbol_sync = symsync_crcf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, settings->symsync_filter_bank_size);
  if (result->symbol_sync == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  symsync_crcf_set_output_rate(result->symbol_sync, 1);
  result->symsync_output_len = rx_max_input_buffer_length;
  result->symsync_output = malloc(sizeof(float complex) * result->symsync_output_len);
  if (result->symsync_output == NULL) {
    bpsk_modem_destroy(result);
    return -ENOMEM;
  }

  result->costas = nco_crcf_create(LIQUID_NCO);
  if (result->costas == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  nco_crcf_pll_set_bandwidth(result->costas, settings->costas_bandwidth);

  if (settings->type == BPSK) {
    result->mod = modemcf_create(LIQUID_MODEM_BPSK);
  } else if (settings->type == DPSK) {
    result->mod = modemcf_create(LIQUID_MODEM_DPSK2);
  } else if (settings->type == SDPSK) {
    result->mod = NULL;
  } else {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  if (result->mod == NULL && settings->type != SDPSK) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  result->tx_prev_symbol = 1.0f + 0.0f * I;
  result->rx_prev_symbol = 1.0f + 0.0f * I;

  result->bit_output_len = rx_max_input_buffer_length;
  result->bit_output = malloc(sizeof(int8_t) * result->bit_output_len);
  if (result->bit_output == NULL) {
    bpsk_modem_destroy(result);
    return -ENOMEM;
  }

  //////// TX chain

  result->max_modulation_input_bits = (size_t) max_input_buffer_length * 8;
  result->max_modulation_buffer_length = result->max_modulation_input_bits * sps;
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    bpsk_modem_destroy(result);
    return -ENOMEM;
  }

  result->interp = firinterp_crcf_create_prototype(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, 0.0f);
  if (result->interp == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }


  if (needs_resampling) {
    double resample_rate_tx = (double) settings->sample_rate / (double) internal_sample_rate;
    result->resampler_tx = msresamp_crcf_create((float) resample_rate_tx, BPSK_MODEM_RESAMPLER_STOPBAND_ATTENUATION_DB);
    if (result->resampler_tx == NULL) {
      bpsk_modem_destroy(result);
      return -ENOMEM;
    }
    result->resampler_tx_output_len = (size_t) ceil((double) result->max_modulation_buffer_length * resample_rate_tx) + BPSK_MODEM_RESAMPLER_OUTPUT_MARGIN;
    result->resampler_tx_output = malloc(sizeof(float complex) * result->resampler_tx_output_len);
    if (result->resampler_tx_output == NULL) {
      bpsk_modem_destroy(result);
      return -ENOMEM;
    }
  }

  *modem = result;
  return 0;
}

int bpsk_modem_set_debug_constellation_file(const char *debug_constellation_file, bpsk_modem *modem) {
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

  float complex *buffer = malloc(sizeof(float complex) * modem->symsync_output_len);
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
  modem->debug_constellation_len = modem->symsync_output_len;
  modem->debug_constellation_file = file;
  return 0;
}

size_t bpsk_modem_max_modulation_buffer_length(void *modem_v) {
  bpsk_modem *modem = modem_v;
  return modem->resampler_tx != NULL ? modem->resampler_tx_output_len : modem->max_modulation_buffer_length;
}

void bpsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem) {
  bpsk_modem *demod = modem;
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

  if (demod->lowpass_filter != NULL) {
    firfilt_crcf_execute_block(demod->lowpass_filter, (float complex *) input, (unsigned int) input_len, demod->lowpass_output);
    input = demod->lowpass_output;
  }

  agc_crcf_execute_block(demod->rx_agc, (float complex *) input, (unsigned int) input_len, demod->agc_output);
  input = demod->agc_output;

  unsigned int num_symbols = 0;
  symsync_crcf_execute(demod->symbol_sync, (float complex *) input, (unsigned int) input_len, demod->symsync_output, &num_symbols);
  input = demod->symsync_output;

  for (unsigned int i = 0; i < num_symbols; i++) {
    float complex mixed;
    nco_crcf_mix_down(demod->costas, input[i], &mixed);

    if (demod->debug_constellation != NULL) {
      demod->debug_constellation[i] = mixed;
    }

    float phase_error;
    if (demod->type == SDPSK) {
      // SYMMETRIC_DIFFERENTIAL rotates by +-90 degrees every symbol, so the constellation
      // lands on all 4 axis points (0/90/180/270) rather than just 2. A plain BPSK (M=2)
      // detector would get a data-dependent sign flip on the 90/270 points, so raise to the
      // 4th power instead: 4*data_phase is always a multiple of 360 degrees, so this cancels
      // the data modulation the same way squaring does for 2-point BPSK.
      // use the angle of the 4th power (in [-45, 45] degrees) instead of its imaginary part:
      // imag(z^4) has a gain of 4*|z|^4, which overdrives the loop during startup transients
      // (e.g. the lowpass filter and AGC settling). the loop can then get pulled onto a false
      // lock at a frequency offset of +-90 degrees/symbol, where the recovered symbols are
      // real-valued (bits are ~random) or +-180 degrees/symbol (bits are inverted). both are
      // valid zero-error points of a 4th-power detector.
      float complex squared = mixed * mixed;
      float complex fourth = squared * squared;
      phase_error = cargf(fourth) / 4.0f;
    } else {
      // costas (M=2) phase error detector: on a two-point (BPSK/DPSK2) constellation this is
      // data-independent, so it tracks the residual carrier without needing hard decisions first.
      // use the angle of the square (in [-90, 90] degrees) instead of real*imag: real*imag has a
      // gain of |z|^2/2, so the loop gain follows the signal amplitude (lowpass filter, AGC and
      // symbol sync transients) and the loop sometimes fails to lock
      phase_error = cargf(mixed * mixed) / 2.0f;
    }
    nco_crcf_pll_step(demod->costas, phase_error);
    nco_crcf_step(demod->costas);

    if (demod->type == SDPSK) {
      // no modemcf involved for this type (see field comment above), so the soft metric is
      // derived directly from the differential detector: cimagf(diff) is ~+-1 for a confident
      // decision and shrinks toward 0 near the decision boundary, same shape as a soft bit
      float complex diff = mixed * conjf(demod->rx_prev_symbol);
      float soft = cimagf(diff) * 127.0f;
      if (soft > 127.0f) {
        soft = 127.0f;
      } else if (soft < -127.0f) {
        soft = -127.0f;
      }
      demod->bit_output[i] = (int8_t) soft;
      demod->rx_prev_symbol = mixed;
    } else {
      unsigned int sym = 0;
      unsigned char soft_bit = 0;
      modemcf_demodulate_soft(demod->mod, mixed, &sym, &soft_bit);
      // liquid-dsp's soft bit is unsigned [0,255] with 255 = confident 1; recenter to signed
      // int8_t range so the sign alone recovers the hard decision, matching the branch above
      demod->bit_output[i] = (int8_t) ((int) soft_bit - 128);
    }
  }

  if (demod->debug_constellation_file != NULL && demod->debug_constellation != NULL) {
    fwrite(demod->debug_constellation, sizeof(float complex), num_symbols, demod->debug_constellation_file);
  }

  *output = demod->bit_output;
  *output_len = num_symbols;
}

void bpsk_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *mod_v) {
  bpsk_modem *mod = mod_v;
  if (input_len * 8 > mod->max_modulation_input_bits) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, mod->max_modulation_input_bits / 8);
    *output = NULL;
    *output_len = 0;
    return;
  }

  size_t sample_index = 0;
  for (size_t i = 0; i < input_len; i++) {
    for (int bit = 0; bit < 8; bit++) {
      unsigned int sym = (input[i] >> (7 - bit)) & 1U;
      float complex symbol;
      if (mod->type == SDPSK) {
        // bit 1 -> +90 degrees, bit 0 -> -90 degrees relative to the previous symbol
        float complex rotation = sym ? (0.0f + 1.0f * I) : (0.0f - 1.0f * I);
        symbol = mod->tx_prev_symbol * rotation;
        mod->tx_prev_symbol = symbol;
      } else {
        modemcf_modulate(mod->mod, sym, &symbol);
      }
      if (mod->debug_constellation_file != NULL) {
        fwrite(&symbol, sizeof(float complex), 1, mod->debug_constellation_file);
      }
      firinterp_crcf_execute(mod->interp, symbol, mod->modulation_output + sample_index);
      sample_index += mod->samples_per_symbol;
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

void bpsk_modem_destroy(void *modem_v) {
  bpsk_modem *modem = modem_v;
  if (modem == NULL) {
    return;
  }
  if (modem->lowpass_filter != NULL) {
    firfilt_crcf_destroy(modem->lowpass_filter);
  }
  if (modem->lowpass_output != NULL) {
    free(modem->lowpass_output);
  }
  if (modem->rx_agc != NULL) {
    agc_crcf_destroy(modem->rx_agc);
  }
  if (modem->agc_output != NULL) {
    free(modem->agc_output);
  }
  if (modem->symbol_sync != NULL) {
    symsync_crcf_destroy(modem->symbol_sync);
  }
  if (modem->costas != NULL) {
    nco_crcf_destroy(modem->costas);
  }
  if (modem->mod != NULL) {
    modemcf_destroy(modem->mod);
  }
  if (modem->interp != NULL) {
    firinterp_crcf_destroy(modem->interp);
  }
  if (modem->resampler_rx != NULL) {
    msresamp_crcf_destroy(modem->resampler_rx);
  }
  if (modem->resampler_rx_output != NULL) {
    free(modem->resampler_rx_output);
  }
  if (modem->resampler_tx != NULL) {
    msresamp_crcf_destroy(modem->resampler_tx);
  }
  if (modem->resampler_tx_output != NULL) {
    free(modem->resampler_tx_output);
  }
  if (modem->symsync_output != NULL) {
    free(modem->symsync_output);
  }
  if (modem->bit_output != NULL) {
    free(modem->bit_output);
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
