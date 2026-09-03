#include "psk_pm_modem.h"
#include "bpsk_modem.h"
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <liquid/liquid.h>
#include <complex.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// PCM/PSK/PM: data BPSK-modulates a subcarrier (handled entirely by an internal bpsk_modem, which
// already provides RRC pulse shaping, symbol timing recovery and its own costas loop for the
// subcarrier), and that subcarrier waveform phase-modulates the RF carrier. This wrapper adds only
// the two things bpsk_modem doesn't do: the outer PM (de)modulation onto/off of the RF carrier and
// the up/down mixing between the subcarrier's real baseband tone and DC.
struct psk_pm_modem_t {
  float modulation_index;
  size_t max_input_buffer_length;

  bpsk_modem *subcarrier_modem;

  // rx only: tracks and removes the RF carrier's residual frequency/phase, so that the (de-rotated
  // signal's) phase is left carrying only the deviation caused by the subcarrier waveform, i.e. a
  // PLL-based phase discriminator. its loop bandwidth must be much narrower than
  // subcarrier_frequency/sample_rate so it tracks only slow carrier drift, not the subcarrier
  // modulation itself.
  nco_crcf carrier_pll;

  // free-running oscillator at subcarrier_frequency: mixes the real subcarrier waveform up to a
  // real passband signal on tx, and mixes the recovered real phase-discriminator output back down
  // to complex baseband (which is then just an ordinary BPSK signal, fed to subcarrier_modem) on rx.
  // frequency/phase uncertainty left after this fixed-frequency mix is absorbed by
  // subcarrier_modem's own costas loop and symbol synchronizer, the same way it absorbs those for
  // a directly-transmitted BPSK signal.
  nco_crcf subcarrier_nco;

  float complex *baseband_output; // rx: subcarrier signal downconverted to complex baseband
  size_t baseband_output_len;

  float complex *modulation_output; // tx: final phase-modulated I/Q
  size_t max_modulation_buffer_length;
};

int psk_pm_modem_create(const psk_pm_modem_settings *settings, uint32_t max_input_buffer_length, const char *debug_constellation_file, psk_pm_modem **modem) {
  if (settings->subcarrier_frequency == 0) {
    fprintf(stderr, "<3>psk/pm modem: subcarrier_frequency must not be 0\n");
    return -EINVAL;
  }
  if ((uint64_t) settings->subcarrier_frequency * 2 >= settings->sample_rate) {
    fprintf(stderr, "<3>psk/pm modem: subcarrier_frequency (%u) must be well below sample_rate/2 (%llu)\n", settings->subcarrier_frequency, (unsigned long long) (settings->sample_rate / 2));
    return -EINVAL;
  }
  if (settings->modulation_index <= 0.0f) {
    fprintf(stderr, "<3>psk/pm modem: modulation_index must be > 0\n");
    return -EINVAL;
  }
  if (settings->carrier_pll_bandwidth <= 0.0f) {
    fprintf(stderr, "<3>psk/pm modem: carrier_pll_bandwidth must be > 0\n");
    return -EINVAL;
  }

  struct psk_pm_modem_t *result = malloc(sizeof(struct psk_pm_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct psk_pm_modem_t){0};
  result->modulation_index = settings->modulation_index;
  result->max_input_buffer_length = max_input_buffer_length;

  bpsk_modem_settings subcarrier_settings = {0};
  subcarrier_settings.sample_rate = settings->sample_rate;
  subcarrier_settings.baud_rate = settings->baud_rate;
  subcarrier_settings.rrc_beta = settings->rrc_beta;
  subcarrier_settings.rrc_delay = settings->rrc_delay;
  subcarrier_settings.costas_bandwidth = settings->costas_bandwidth;
  subcarrier_settings.symsync_filter_bank_size = settings->symsync_filter_bank_size;
  subcarrier_settings.type = NORMAL;
  int code = bpsk_modem_create(&subcarrier_settings, max_input_buffer_length, debug_constellation_file, &result->subcarrier_modem);
  if (code != 0) {
    psk_pm_modem_destroy(result);
    return code;
  }

  result->carrier_pll = nco_crcf_create(LIQUID_NCO);
  if (result->carrier_pll == NULL) {
    psk_pm_modem_destroy(result);
    return -EINVAL;
  }
  nco_crcf_pll_set_bandwidth(result->carrier_pll, settings->carrier_pll_bandwidth);

  result->subcarrier_nco = nco_crcf_create(LIQUID_VCO);
  if (result->subcarrier_nco == NULL) {
    psk_pm_modem_destroy(result);
    return -EINVAL;
  }
  nco_crcf_set_frequency(result->subcarrier_nco, 2.0f * (float) M_PI * (float) settings->subcarrier_frequency / (float) settings->sample_rate);

  result->baseband_output_len = max_input_buffer_length;
  result->baseband_output = malloc(sizeof(float complex) * result->baseband_output_len);
  if (result->baseband_output == NULL) {
    psk_pm_modem_destroy(result);
    return -ENOMEM;
  }

  result->max_modulation_buffer_length = bpsk_modem_max_modulation_buffer_length(result->subcarrier_modem);
  result->modulation_output = malloc(sizeof(float complex) * result->max_modulation_buffer_length);
  if (result->modulation_output == NULL) {
    psk_pm_modem_destroy(result);
    return -ENOMEM;
  }

  *modem = result;
  return 0;
}

size_t psk_pm_modem_max_modulation_buffer_length(void *modem_v) {
  psk_pm_modem *modem = modem_v;
  return modem->max_modulation_buffer_length;
}

void psk_pm_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *demod_v) {
  psk_pm_modem *demod = demod_v;
  if (input_len > demod->max_input_buffer_length) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, demod->max_input_buffer_length);
    *output = NULL;
    *output_len = 0;
    return;
  }

  for (size_t i = 0; i < input_len; i++) {
    // de-rotate by the tracked carrier phase/frequency. once locked, the angle of the result is
    // the RF carrier's phase deviation, i.e. modulation_index times the subcarrier waveform
    float complex derotated;
    nco_crcf_mix_down(demod->carrier_pll, input[i], &derotated);
    float discriminator = atan2f(cimagf(derotated), crealf(derotated));
    nco_crcf_pll_step(demod->carrier_pll, discriminator);
    nco_crcf_step(demod->carrier_pll);

    // mix the recovered real subcarrier waveform down to complex baseband. any residual
    // frequency/phase error here (subcarrier_nco is free-running, not itself a PLL) is absorbed by
    // subcarrier_modem's own costas loop below, same as bpsk_modem does for a direct RF BPSK signal
    float complex baseband;
    nco_crcf_mix_down(demod->subcarrier_nco, discriminator, &baseband);
    nco_crcf_step(demod->subcarrier_nco);
    demod->baseband_output[i] = baseband;
  }

  bpsk_modem_demodulate(demod->baseband_output, input_len, output, output_len, demod->subcarrier_modem);
}

void psk_pm_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *mod_v) {
  psk_pm_modem *mod = mod_v;

  float complex *subcarrier_symbols = NULL;
  size_t subcarrier_symbols_len = 0;
  bpsk_modem_modulate(input, input_len, &subcarrier_symbols, &subcarrier_symbols_len, mod->subcarrier_modem);
  if (subcarrier_symbols == NULL) {
    *output = NULL;
    *output_len = 0;
    return;
  }

  for (size_t i = 0; i < subcarrier_symbols_len; i++) {
    // subcarrier_symbols is the RRC-shaped BPSK baseband waveform (real-valued: BPSK's constellation
    // sits on the real axis and the RRC taps are real, so the imaginary part is always exactly 0).
    // multiplying by the subcarrier tone's cosine turns it into the real, band-passed subcarrier
    // signal, which then directly phase-modulates the carrier.
    float subcarrier_sample = crealf(subcarrier_symbols[i]) * nco_crcf_cos(mod->subcarrier_nco);
    nco_crcf_step(mod->subcarrier_nco);

    float phase = mod->modulation_index * subcarrier_sample;
    mod->modulation_output[i] = cosf(phase) + I * sinf(phase);
  }

  *output = mod->modulation_output;
  *output_len = subcarrier_symbols_len;
}

void psk_pm_modem_destroy(void *modem_v) {
  psk_pm_modem *modem = modem_v;
  if (modem == NULL) {
    return;
  }
  if (modem->subcarrier_modem != NULL) {
    bpsk_modem_destroy(modem->subcarrier_modem);
  }
  if (modem->carrier_pll != NULL) {
    nco_crcf_destroy(modem->carrier_pll);
  }
  if (modem->subcarrier_nco != NULL) {
    nco_crcf_destroy(modem->subcarrier_nco);
  }
  if (modem->baseband_output != NULL) {
    free(modem->baseband_output);
  }
  if (modem->modulation_output != NULL) {
    free(modem->modulation_output);
  }
  free(modem);
}
