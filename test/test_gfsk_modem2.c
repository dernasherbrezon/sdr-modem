#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unity.h>
#include "../src/dsp/gfsk_modem2.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BAUD_RATE 4800
#define INPUT_LEN 128
// tx gaussian pulse, rx matched filter, resamplers and symsync add a few symbols of delay.
// exact value depends on the configuration, so search for it instead of hardcoding
#define MAX_BIT_LAG 40
// let the agc and the symbol timing loop settle
#define SKIP_BITS 300
// noiseless loopback: everything after the loops have settled should be recovered
#define MAX_MISMATCH_RATIO 0.02

gfsk_modem2 *mod = NULL;
gfsk_modem2 *demod = NULL;
uint8_t *mod_input = NULL;

static GfskModemSettings default_settings(uint64_t sample_rate, uint32_t deviation) {
  GfskModemSettings settings = GFSK_MODEM_SETTINGS__INIT;
  settings.sample_rate = sample_rate;
  settings.baud_rate = BAUD_RATE;
  settings.deviation = deviation;
  settings.bandwidth = 0;
  settings.bt = 0.5f;
  settings.use_dc_block = true;
  return settings;
}

static void setup_random_input(size_t len) {
  mod_input = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(mod_input != NULL);
  // own generator so that the data is the same on every platform
  uint32_t state = 0x2545F491;
  for (size_t i = 0; i < len; i++) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    mod_input[i] = (uint8_t) (state >> 8);
  }
}

static unsigned int input_bit(size_t index) {
  return (mod_input[index / 8] >> (7 - (index % 8))) & 1U;
}

// fraction of demodulated hard decisions that differ from the input bits, assuming the output is
// delayed by lag bits
static double mismatch_ratio(const int8_t *output, size_t output_len, size_t lag) {
  size_t total = 0;
  size_t mismatches = 0;
  for (size_t i = SKIP_BITS; i < output_len; i++) {
    size_t expected_index = i - lag;
    if (expected_index >= INPUT_LEN * 8) {
      break;
    }
    unsigned int actual_bit = output[i] >= 0 ? 1U : 0U;
    if (actual_bit != input_bit(expected_index)) {
      mismatches++;
    }
    total++;
  }
  TEST_ASSERT_GREATER_THAN_size_t(50, total);
  return (double) mismatches / (double) total;
}

static double best_mismatch_ratio(const int8_t *output, size_t output_len) {
  double best = 1.0;
  for (size_t lag = 0; lag <= MAX_BIT_LAG; lag++) {
    double ratio = mismatch_ratio(output, output_len, lag);
    if (ratio < best) {
      best = ratio;
    }
  }
  return best;
}

static void round_trip(uint64_t sample_rate, uint32_t deviation, double carrier_offset_hz) {
  GfskModemSettings settings = default_settings(sample_rate, deviation);

  int code = gfsk_modem2_create(&settings, settings.sample_rate, INPUT_LEN, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  // demodulator has to accept everything the modulator can produce
  uint32_t max_samples = (uint32_t) gfsk_modem2_max_modulation_buffer_length(mod);
  code = gfsk_modem2_create(&settings, settings.sample_rate, max_samples, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_random_input(INPUT_LEN);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  gfsk_modem2_modulate(mod_input, INPUT_LEN, &modulated, &modulated_len, mod);
  TEST_ASSERT(modulated != NULL);
  TEST_ASSERT(modulated_len > 0);
  TEST_ASSERT(modulated_len <= max_samples);
  // roughly sample_rate / baud_rate samples per bit. resampler and its filter can be off by a
  // few samples
  uint64_t expected_len = (uint64_t) INPUT_LEN * 8 * sample_rate / BAUD_RATE;
  TEST_ASSERT_UINT64_WITHIN(expected_len / 50 + 32, expected_len, modulated_len);

  if (carrier_offset_hz != 0.0) {
    // sdr and transmitter clocks are not perfectly in sync: shows up as dc in the frequency
    for (size_t i = 0; i < modulated_len; i++) {
      modulated[i] *= cexpf(I * (float) (2.0 * M_PI * carrier_offset_hz * (double) i / (double) sample_rate + 0.7));
    }
  }

  int8_t *output = NULL;
  size_t output_len = 0;
  gfsk_modem2_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT(output != NULL);
  TEST_ASSERT_GREATER_THAN_size_t(SKIP_BITS + 100, output_len);
  // can't recover more symbols than were sent
  TEST_ASSERT(output_len <= INPUT_LEN * 8 + 1);

  TEST_ASSERT_TRUE_MESSAGE(best_mismatch_ratio(output, output_len) <= MAX_MISMATCH_RATIO, "too many bit errors");
}

// deviation is baud_rate / 4 (h = 0.5, i.e. gmsk) and baud_rate / 2 (h = 1)
void test_exact_sps4_h05() { round_trip(4 * BAUD_RATE, BAUD_RATE / 4, 0); }
void test_exact_sps4_h1() { round_trip(4 * BAUD_RATE, BAUD_RATE / 2, 0); }
void test_exact_sps10_h05() { round_trip(10 * BAUD_RATE, BAUD_RATE / 4, 0); }
void test_exact_sps10_h1() { round_trip(10 * BAUD_RATE, BAUD_RATE / 2, 0); }
void test_exact_sps5_h1() { round_trip(5 * BAUD_RATE, BAUD_RATE / 2, 0); }
void test_exact_sps20_h1() { round_trip(20 * BAUD_RATE, BAUD_RATE / 2, 0); }
void test_fractional_sps9_h05() { round_trip(44100, BAUD_RATE / 4, 0); }
void test_fractional_sps9_h1() { round_trip(44100, BAUD_RATE / 2, 0); }
void test_carrier_offset_h1() { round_trip(4 * BAUD_RATE, BAUD_RATE / 2, 200.0); }
// half of the deviation: the timing loop fails if dc is removed after symsync instead of before
void test_carrier_offset_large_h1() { round_trip(44100, BAUD_RATE / 2, BAUD_RATE / 4); }
void test_carrier_offset_large_h05() { round_trip(4 * BAUD_RATE, BAUD_RATE / 4, BAUD_RATE / 8); }
void test_carrier_offset_fractional_h05() { round_trip(44100, BAUD_RATE / 4, -150.0); }

// a tone at +deviation is a run of 1 bits. output must be positive and close to full scale, for any
// modulation index (h = 1 here: a symbol-spaced phase detector would be ambiguous)
void test_soft_symbols_scale() {
  GfskModemSettings settings = default_settings(4 * BAUD_RATE, BAUD_RATE / 2);
  settings.use_dc_block = false;
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, settings.sample_rate, 4096, &demod));
  size_t len = 4096;
  float complex *input = malloc(sizeof(float complex) * len);
  TEST_ASSERT_NOT_NULL(input);
  for (size_t i = 0; i < len; i++) {
    input[i] = cexpf(I * (float) (2.0 * M_PI * (BAUD_RATE / 2.0) * (double) i / (double) settings.sample_rate));
  }
  int8_t *output = NULL;
  size_t output_len = 0;
  gfsk_modem2_demodulate(input, len, &output, &output_len, demod);
  free(input);
  TEST_ASSERT_GREATER_THAN_size_t(500, output_len);
  for (size_t i = output_len - 100; i < output_len; i++) {
    TEST_ASSERT_INT8_WITHIN(3, 127, output[i]);
  }
}

// modulated tone frequency must match the configured deviation: a stream of 1 bits at +deviation
void test_modulation_deviation() {
  GfskModemSettings settings = default_settings(8 * BAUD_RATE, 3000);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, settings.sample_rate, 64, &mod));
  uint8_t ones[64];
  memset(ones, 0xFF, sizeof(ones));
  float complex *output = NULL;
  size_t output_len = 0;
  gfsk_modem2_modulate(ones, sizeof(ones), &output, &output_len, mod);
  TEST_ASSERT_EQUAL_size_t(64 * 8 * 8, output_len);
  // steady state: skip the leading transient
  float complex *tail = output + output_len / 2;
  double sum = 0;
  size_t count = output_len / 2 - 1;
  for (size_t i = 0; i < count; i++) {
    sum += cargf(conjf(tail[i]) * tail[i + 1]);
  }
  double frequency = sum / (double) count * settings.sample_rate / (2 * M_PI);
  TEST_ASSERT_FLOAT_WITHIN(30.0f, 3000.0f, (float) frequency);
  // constant envelope
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, cabsf(tail[100]));
}

static void assert_create_fails(uint64_t sample_rate, uint32_t baud_rate, uint32_t deviation, float bt) {
  GfskModemSettings settings = default_settings(sample_rate, deviation);
  settings.baud_rate = baud_rate;
  settings.bt = bt;
  gfsk_modem2 *created = NULL;
  TEST_ASSERT_EQUAL_INT(-EINVAL, gfsk_modem2_create(&settings, sample_rate, INPUT_LEN, &created));
  TEST_ASSERT_NULL(created);
}

void test_create_invalid_settings() {
  // less than 2 samples per symbol
  assert_create_fails(0, BAUD_RATE, 2400, 0.5f);
  assert_create_fails(BAUD_RATE, BAUD_RATE, 2400, 0.5f);
  assert_create_fails(2 * BAUD_RATE - 1, BAUD_RATE, 2400, 0.5f);
  assert_create_fails(48000, 0, 2400, 0.5f);
  assert_create_fails(48000, BAUD_RATE, 0, 0.5f);
  assert_create_fails(48000, BAUD_RATE, 2400, 0.0f);
  assert_create_fails(48000, BAUD_RATE, 2400, 1.5f);
}

void test_create_smallest_sps() {
  GfskModemSettings settings = default_settings(2 * BAUD_RATE, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, settings.sample_rate, INPUT_LEN, &mod));
}

// rx runs at a different (decimated) rate than the raw tx one
void test_create_rx_rate_differs_from_tx() {
  GfskModemSettings settings = default_settings(192000, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, 24000, INPUT_LEN, &demod));
}

void test_invalid_buffers() {
  GfskModemSettings settings = default_settings(44100, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, settings.sample_rate, INPUT_LEN, &mod));
  float complex *input = calloc(INPUT_LEN + 1, sizeof(float complex));
  TEST_ASSERT_NOT_NULL(input);

  // more samples than the modem was created for: rejected, output is reset
  int8_t sentinel = 0;
  int8_t *output = &sentinel;
  size_t output_len = 99;
  gfsk_modem2_demodulate(input, INPUT_LEN + 1, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // empty buffer is valid
  output = NULL;
  output_len = 99;
  gfsk_modem2_demodulate(input, 0, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
  free(input);

  setup_random_input(INPUT_LEN + 1);
  float complex *modulated = (float complex *) &sentinel;
  size_t modulated_len = 99;
  gfsk_modem2_modulate(mod_input, INPUT_LEN + 1, &modulated, &modulated_len, mod);
  TEST_ASSERT_NULL(modulated);
  TEST_ASSERT_EQUAL_size_t(0, modulated_len);
}

void tearDown() {
  if (mod != NULL) {
    gfsk_modem2_destroy(mod);
    mod = NULL;
  }
  if (demod != NULL) {
    gfsk_modem2_destroy(demod);
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
  RUN_TEST(test_exact_sps4_h05);
  RUN_TEST(test_exact_sps4_h1);
  RUN_TEST(test_exact_sps10_h05);
  RUN_TEST(test_exact_sps10_h1);
  RUN_TEST(test_exact_sps5_h1);
  RUN_TEST(test_exact_sps20_h1);
  RUN_TEST(test_fractional_sps9_h05);
  RUN_TEST(test_fractional_sps9_h1);
  RUN_TEST(test_carrier_offset_h1);
  RUN_TEST(test_carrier_offset_large_h1);
  RUN_TEST(test_carrier_offset_large_h05);
  RUN_TEST(test_carrier_offset_fractional_h05);
  RUN_TEST(test_soft_symbols_scale);
  RUN_TEST(test_modulation_deviation);
  RUN_TEST(test_create_invalid_settings);
  RUN_TEST(test_create_smallest_sps);
  RUN_TEST(test_create_rx_rate_differs_from_tx);
  RUN_TEST(test_invalid_buffers);
  return UNITY_END();
}
