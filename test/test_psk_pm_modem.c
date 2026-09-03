#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
#include "../src/dsp/psk_pm_modem.h"
#include "utils.h"

psk_pm_modem *mod = NULL;
psk_pm_modem *demod = NULL;
uint8_t *mod_input = NULL;

static psk_pm_modem_settings default_settings(uint64_t sample_rate) {
  psk_pm_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = 4800;
  settings.rrc_beta = 0.35f;
  settings.rrc_delay = 5;
  settings.costas_bandwidth = 0.05f;
  settings.symsync_filter_bank_size = 32;
  settings.subcarrier_frequency = 6000;
  settings.modulation_index = 1.0f;
  settings.carrier_pll_bandwidth = 0.001f;
  return settings;
}

void test_invalid_subcarrier_frequency_zero() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.subcarrier_frequency = 0;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, NULL, &mod));
}

void test_invalid_subcarrier_frequency_too_high() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.subcarrier_frequency = 30000;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, NULL, &mod));
}

void test_invalid_modulation_index() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.modulation_index = 0.0f;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, NULL, &mod));
}

void test_invalid_carrier_pll_bandwidth() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.carrier_pll_bandwidth = 0.0f;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, NULL, &mod));
}

void test_round_trip() {
  psk_pm_modem_settings settings = default_settings(48000);
  size_t input_len = 64;
  uint32_t max_buffer_length = 16384;

  int code = psk_pm_modem_create(&settings, (uint32_t) input_len, NULL, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = psk_pm_modem_create(&settings, max_buffer_length, NULL, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  mod_input = malloc(sizeof(uint8_t) * input_len);
  TEST_ASSERT(mod_input != NULL);
  srand(42);
  for (size_t i = 0; i < input_len; i++) {
    mod_input[i] = (uint8_t) rand();
  }

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  psk_pm_modem_modulate(mod_input, input_len, &modulated, &modulated_len, mod);
  TEST_ASSERT(modulated != NULL);
  TEST_ASSERT(modulated_len > 0);
  TEST_ASSERT(modulated_len <= max_buffer_length);

  // a phase-modulated signal has unit envelope
  for (size_t i = 0; i < modulated_len; i++) {
    TEST_ASSERT(fabsf(cabsf(modulated[i]) - 1.0f) < 0.001f);
  }

  int8_t *output = NULL;
  size_t output_len = 0;
  psk_pm_modem_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT(output_len > 0);

  // RRC pulse shaping (tx interpolator + rx matched filter, both with delay rrc_delay symbols)
  // delays the recovered bit stream by 2*rrc_delay bits relative to the input -- a normal property
  // of matched filtering, not something specific to psk_pm_modem
  size_t delay_bits = 2 * settings.rrc_delay;
  // the carrier and subcarrier costas/PLL loops can lock with a 180 degree phase ambiguity, so
  // the recovered bit stream may come out complemented relative to the input; skip the first few
  // dozen symbols while the loops settle, then take whichever polarity has fewer mismatches
  size_t skip_bits = 100;
  TEST_ASSERT(output_len > skip_bits + 100);

  size_t total = 0;
  size_t mismatches_normal = 0;
  for (size_t i = skip_bits; i < output_len; i++) {
    size_t expected_index = i - delay_bits;
    if (expected_index / 8 >= input_len) {
      break;
    }
    unsigned int expected_bit = (mod_input[expected_index / 8] >> (7 - (expected_index % 8))) & 1U;
    unsigned int actual_bit = output[i] >= 0 ? 1U : 0U;
    if (actual_bit != expected_bit) {
      mismatches_normal++;
    }
    total++;
  }
  size_t mismatches_inverted = total - mismatches_normal;
  size_t mismatches = mismatches_normal < mismatches_inverted ? mismatches_normal : mismatches_inverted;

  TEST_ASSERT(total > 0);
  // noiseless loopback: allow a little slack for residual loop settling, but it should be well
  // under 1 in 20 bits
  TEST_ASSERT(mismatches <= total / 20);
}

void tearDown() {
  if (mod != NULL) {
    psk_pm_modem_destroy(mod);
    mod = NULL;
  }
  if (demod != NULL) {
    psk_pm_modem_destroy(demod);
    demod = NULL;
  }
  if (mod_input != NULL) {
    free(mod_input);
    mod_input = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_invalid_subcarrier_frequency_zero);
  RUN_TEST(test_invalid_subcarrier_frequency_too_high);
  RUN_TEST(test_invalid_modulation_index);
  RUN_TEST(test_invalid_carrier_pll_bandwidth);
  RUN_TEST(test_round_trip);
  return UNITY_END();
}
