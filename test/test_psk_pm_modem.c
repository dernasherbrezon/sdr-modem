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
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, &mod));
}

void test_invalid_subcarrier_frequency_too_high() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.subcarrier_frequency = 30000;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, &mod));
}

void test_invalid_modulation_index() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.modulation_index = 0.0f;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, &mod));
}

void test_invalid_carrier_pll_bandwidth() {
  psk_pm_modem_settings settings = default_settings(48000);
  settings.carrier_pll_bandwidth = 0.0f;
  TEST_ASSERT_NOT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, &mod));
}

void test_round_trip() {
  psk_pm_modem_settings settings = default_settings(48000);
  size_t input_len = 64;
  uint32_t max_buffer_length = 16384;

  int code = psk_pm_modem_create(&settings, (uint32_t) input_len, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = psk_pm_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_random_input(&mod_input, input_len);

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

  // no carrier offset was applied
  TEST_ASSERT_FLOAT_WITHIN(50.0f, 0.0f, psk_pm_modem_get_frequency_offset(demod));
}

void test_frequency_offset_before_demodulate() {
  psk_pm_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, psk_pm_modem_create(&settings, 4096, &demod));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, psk_pm_modem_get_frequency_offset(demod));
}

static size_t modulate_and_flush(const uint8_t *input, size_t input_len, size_t chunk_len, float complex *result, size_t result_capacity) {
  size_t total = 0;
  for (size_t offset = 0; offset < input_len; offset += chunk_len) {
    size_t len = input_len - offset < chunk_len ? input_len - offset : chunk_len;
    float complex *output = NULL;
    size_t output_len = 0;
    psk_pm_modem_modulate(input + offset, len, &output, &output_len, mod);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT(total + output_len <= result_capacity);
    memcpy(result + total, output, sizeof(float complex) * output_len);
    total += output_len;
  }
  float complex *output = NULL;
  size_t output_len = 0;
  psk_pm_modem_modulate_flush(&output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT(total + output_len <= result_capacity);
  memcpy(result + total, output, sizeof(float complex) * output_len);
  return total + output_len;
}

static void assert_same_signal(const float complex *expected, size_t expected_len, const float complex *actual, size_t actual_len) {
  TEST_ASSERT_EQUAL_size_t(expected_len, actual_len);
  for (size_t i = 0; i < expected_len; i++) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, crealf(expected[i]), crealf(actual[i]));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, cimagf(expected[i]), cimagf(actual[i]));
  }
}

// the flush resets the tx, including the subcarrier phase: the second transmission is the same as the first
void test_flush_resets_state() {
  psk_pm_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, psk_pm_modem_create(&settings, 16, &mod));
  setup_random_input(&mod_input, 16);
  size_t capacity = psk_pm_modem_max_modulation_buffer_length(mod);
  float complex *first = malloc(sizeof(float complex) * capacity);
  float complex *second = malloc(sizeof(float complex) * capacity);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  size_t first_len = modulate_and_flush(mod_input, 16, 16, first, capacity);
  size_t second_len = modulate_and_flush(mod_input, 16, 16, second, capacity);
  assert_same_signal(first, first_len, second, second_len);
  free(first);
  free(second);
}

// frames one after another without a gap: the same signal as one long frame
void test_streaming_chunks() {
  psk_pm_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, psk_pm_modem_create(&settings, 16, &mod));
  setup_random_input(&mod_input, 16);
  size_t capacity = psk_pm_modem_max_modulation_buffer_length(mod);
  float complex *whole = malloc(sizeof(float complex) * capacity);
  float complex *chunks = malloc(sizeof(float complex) * capacity);
  TEST_ASSERT_NOT_NULL(whole);
  TEST_ASSERT_NOT_NULL(chunks);
  size_t whole_len = modulate_and_flush(mod_input, 16, 16, whole, capacity);
  size_t chunks_len = modulate_and_flush(mod_input, 16, 4, chunks, capacity);
  assert_same_signal(whole, whole_len, chunks, chunks_len);
  free(whole);
  free(chunks);
}

void test_flush_without_modulate() {
  psk_pm_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, psk_pm_modem_create(&settings, 16, &mod));
  float complex *output = NULL;
  size_t output_len = 123;
  psk_pm_modem_modulate_flush(&output, &output_len, mod);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
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
  RUN_TEST(test_frequency_offset_before_demodulate);
  RUN_TEST(test_flush_resets_state);
  RUN_TEST(test_streaming_chunks);
  RUN_TEST(test_flush_without_modulate);
  return UNITY_END();
}
