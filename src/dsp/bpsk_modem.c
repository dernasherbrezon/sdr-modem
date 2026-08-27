#include "bpsk_modem.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <liquid/liquid.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct bpsk_modem_t {
  uint64_t sample_rate;
  uint32_t baud_rate;
  unsigned int samples_per_symbol;

  // matched RRC filter + symbol timing recovery (Mueller & Muller-style timing error detector),
  // realized from an RRC prototype internally by liquid-dsp
  symsync_crcf symbol_sync;
  float complex *symsync_output;
  size_t symsync_output_len;

  // carrier recovery: tracks and corrects small residual frequency/phase offsets left after
  // the symbol synchronizer, one correction per recovered symbol
  nco_crcf costas;

  bpsk_modem_type type;
  modemcf mod;

  // SYMMETRIC_DIFFERENTIAL only: liquid-dsp has no built-in modem for it, so bits are
  // encoded/decoded as +-90 degree rotations relative to the previous symbol instead of
  // going through modemcf. rx_prev_symbol carries the previously received (noisy) symbol
  // for symbol-by-symbol differential detection.
  float complex tx_prev_symbol;
  float complex rx_prev_symbol;

  int8_t *bit_output;
  size_t output_len;

  // TX chain:
  firinterp_crcf interp;
  size_t max_modulation_input_bits;
  size_t max_modulation_buffer_length;
  float complex *modulation_output;

  FILE *debug_constellation_file;
};

int bpsk_modem_create(const bpsk_modem_settings *settings, uint32_t max_input_buffer_length, const char *debug_constellation_file, bpsk_modem **modem) {
  if (settings->baud_rate == 0 || settings->sample_rate % settings->baud_rate != 0) {
    fprintf(stderr, "<3>bpsk modem: sample_rate (%llu) must be an integer multiple of baud_rate (%u)\n", (unsigned long long) settings->sample_rate, settings->baud_rate);
    return -EINVAL;
  }
  unsigned int sps = (unsigned int) (settings->sample_rate / settings->baud_rate);
  if (sps < 2) {
    fprintf(stderr, "<3>bpsk modem: samples per symbol (%u) must be at least 2\n", sps);
    return -EINVAL;
  }

  struct bpsk_modem_t *result = malloc(sizeof(struct bpsk_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct bpsk_modem_t){0};
  result->sample_rate = settings->sample_rate;
  result->baud_rate = settings->baud_rate;
  result->samples_per_symbol = sps;
  result->type = settings->type;

  result->symbol_sync = symsync_crcf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, settings->symsync_filter_bank_size);
  if (result->symbol_sync == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  symsync_crcf_set_output_rate(result->symbol_sync, 1);

  result->costas = nco_crcf_create(LIQUID_NCO);
  if (result->costas == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  nco_crcf_pll_set_bandwidth(result->costas, settings->costas_bandwidth);

  if (settings->type == NORMAL) {
    result->mod = modemcf_create(LIQUID_MODEM_BPSK);
  } else if (settings->type == DIFFERENTIAL) {
    result->mod = modemcf_create(LIQUID_MODEM_DPSK2);
  } else if (settings->type == SYMMETRIC_DIFFERENTIAL) {
    result->mod = NULL;
  } else {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  if (result->mod == NULL && settings->type != SYMMETRIC_DIFFERENTIAL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  result->tx_prev_symbol = 1.0f + 0.0f * I;
  result->rx_prev_symbol = 1.0f + 0.0f * I;

  // one output symbol for every sps input samples, at most
  result->output_len = max_input_buffer_length;
  result->symsync_output_len = max_input_buffer_length;
  result->symsync_output = malloc(sizeof(float complex) * result->symsync_output_len);
  result->bit_output = malloc(sizeof(int8_t) * result->output_len);

  result->interp = firinterp_crcf_create_prototype(LIQUID_FIRFILT_RRC, sps, settings->rrc_delay, settings->rrc_beta, 0.0f);
  if (result->interp == NULL) {
    bpsk_modem_destroy(result);
    return -EINVAL;
  }
  result->max_modulation_input_bits = (size_t) max_input_buffer_length * 8;
  result->max_modulation_buffer_length = result->max_modulation_input_bits * sps;
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    bpsk_modem_destroy(result);
    return -ENOMEM;
  }

  if (debug_constellation_file != NULL) {
    result->debug_constellation_file = fopen(debug_constellation_file, "wb");
    if (result->debug_constellation_file == NULL) {
      fprintf(stderr, "<3>unable to open debug constellation file: %s\n", debug_constellation_file);
      bpsk_modem_destroy(result);
      return -1;
    }
  }

  *modem = result;
  return 0;
}

size_t bpsk_modem_max_modulation_buffer_length(void *modem_v) {
  bpsk_modem *modem = modem_v;
  return modem->max_modulation_buffer_length;
}

void bpsk_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *demod_v) {
  bpsk_modem *demod = demod_v;
  if (input_len > demod->symsync_output_len) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, demod->symsync_output_len);
    *output = NULL;
    *output_len = 0;
    return;
  }

  unsigned int num_symbols = 0;
  symsync_crcf_execute(demod->symbol_sync, (float complex *) input, (unsigned int) input_len, demod->symsync_output, &num_symbols);

  if (demod->debug_constellation_file != NULL) {
    fwrite(demod->symsync_output, sizeof(float complex), num_symbols, demod->debug_constellation_file);
  }

  for (unsigned int i = 0; i < num_symbols; i++) {
    float complex mixed;
    nco_crcf_mix_down(demod->costas, demod->symsync_output[i], &mixed);

    float phase_error;
    if (demod->type == SYMMETRIC_DIFFERENTIAL) {
      // SYMMETRIC_DIFFERENTIAL rotates by +-90 degrees every symbol, so the constellation
      // lands on all 4 axis points (0/90/180/270) rather than just 2. A plain BPSK (M=2)
      // detector would get a data-dependent sign flip on the 90/270 points, so raise to the
      // 4th power instead: 4*data_phase is always a multiple of 360 degrees, so this cancels
      // the data modulation the same way squaring does for 2-point BPSK.
      float complex squared = mixed * mixed;
      float complex fourth = squared * squared;
      phase_error = cimagf(fourth);
    } else {
      // costas (M=2) phase error detector: on a two-point (BPSK/DPSK2) constellation this is
      // data-independent, so it tracks the residual carrier without needing hard decisions first
      phase_error = crealf(mixed) * cimagf(mixed);
    }
    nco_crcf_pll_step(demod->costas, phase_error);
    nco_crcf_step(demod->costas);

    if (demod->type == SYMMETRIC_DIFFERENTIAL) {
      float complex diff = mixed * conjf(demod->rx_prev_symbol);
      demod->bit_output[i] = (cimagf(diff) > 0.0f) ? 1 : 0;
      demod->rx_prev_symbol = mixed;
    } else {
      unsigned int sym = 0;
      modemcf_demodulate(demod->mod, mixed, &sym);
      demod->bit_output[i] = (int8_t) sym;
    }
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
      if (mod->type == SYMMETRIC_DIFFERENTIAL) {
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
}

void bpsk_modem_destroy(void *modem_v) {
  bpsk_modem *modem = modem_v;
  if (modem == NULL) {
    return;
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
  free(modem);
}
