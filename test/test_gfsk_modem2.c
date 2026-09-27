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
// tx gaussian pulse, rx filters and resamplers add a few symbols of delay before the sync word.
// exact value depends on the configuration, so search for it instead of hardcoding
#define MAX_BIT_LAG 40
// timing and frequency come from the sync word, so there is nothing to settle
#define SKIP_BITS MAX_BIT_LAG
// noiseless loopback: everything from the sync word on should be recovered
#define MAX_MISMATCH_RATIO 0.02

// ais training sequence + hdlc flag, on-air
static const uint32_t SYNC_WORD = 0xCCCCCCFE;
#define SYNC_WORD_BITS (sizeof(SYNC_WORD) * 8)
// nrzi can invert the polarity of the whole burst
static const uint8_t SYNC_WORD_INVERTED[] = {0x33, 0x33, 0x33, 0x01};

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

  int code = gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, INPUT_LEN, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  // demodulator has to accept everything the modulator can produce
  uint32_t max_samples = (uint32_t) gfsk_modem2_max_modulation_buffer_length(mod);
  code = gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, max_samples, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_random_input(INPUT_LEN);
  memcpy(mod_input, &SYNC_WORD, sizeof(SYNC_WORD));

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
// half of the deviation: estimated from the sync word
void test_carrier_offset_large_h1(){ round_trip(44100, BAUD_RATE / 2, BAUD_RATE / 4); }
void test_carrier_offset_large_h05() { round_trip(4 * BAUD_RATE, BAUD_RATE / 4, BAUD_RATE / 8); }
void test_carrier_offset_fractional_h05() { round_trip(44100, BAUD_RATE / 4, -150.0); }

// a tone at +deviation is a run of 1 bits. no sync word, so the output is scaled by the configured
// deviation: positive and close to full scale, for any modulation index (h = 1 here)
void test_soft_symbols_scale() {
  GfskModemSettings settings = default_settings(4 * BAUD_RATE, BAUD_RATE / 2);
  settings.use_dc_block = false;
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, 4096, &demod));
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

// once the sync word is found, soft symbols are normalized by its amplitude: full scale even if the
// configured deviation is twice the actual one
void test_soft_symbols_normalized_by_sync_word() {
  GfskModemSettings settings = default_settings(8 * BAUD_RATE, BAUD_RATE / 4);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, INPUT_LEN, &mod));
  settings.deviation = BAUD_RATE / 2;
  uint32_t max_samples = (uint32_t) gfsk_modem2_max_modulation_buffer_length(mod);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, max_samples, &demod));

  uint8_t input[4 + 16 + 16 + 8];
  memcpy(input, &SYNC_WORD, sizeof(SYNC_WORD));
  memset(input + 4, 0xFF, 16);
  memset(input + 4 + 16, 0x00, 16);
  // flush the demodulator lag
  memset(input + 4 + 32, 0x55, 8);
  float complex *modulated = NULL;
  size_t modulated_len = 0;
  gfsk_modem2_modulate(input, sizeof(input), &modulated, &modulated_len, mod);
  int8_t *output = NULL;
  size_t output_len = 0;
  gfsk_modem2_demodulate(modulated, modulated_len, &output, &output_len, demod);

  size_t positive = 0;
  size_t negative = 0;
  for (size_t i = 0; i < output_len; i++) {
    if (output[i] >= 115) {
      positive++;
    } else if (output[i] <= -115) {
      negative++;
    }
  }
  // runs of 16 bytes, minus the transitions at the edges
  TEST_ASSERT_GREATER_THAN_size_t(120, positive);
  TEST_ASSERT_GREATER_THAN_size_t(120, negative);
}

static uint32_t noise_state = 0x1234567;

static float noise_uniform() {
  noise_state ^= noise_state << 13;
  noise_state ^= noise_state >> 17;
  noise_state ^= noise_state << 5;
  return ((float) (noise_state >> 8) + 0.5f) / 16777216.0f;
}

static float complex noise_sample(float sigma) {
  float r = sigma * sqrtf(-2.0f * logf(noise_uniform()));
  float phase = 2.0f * (float) M_PI * noise_uniform();
  return r * cexpf(I * phase);
}

static size_t append_noise(float complex *signal, size_t offset, size_t len, float sigma) {
  for (size_t i = 0; i < len; i++) {
    signal[offset + i] = noise_sample(sigma);
  }
  return offset + len;
}

static size_t append_burst(const uint8_t *burst, size_t burst_len, double carrier_offset_hz, float amplitude, float sigma, uint64_t sample_rate, float complex *signal, size_t offset) {
  float complex *modulated = NULL;
  size_t modulated_len = 0;
  gfsk_modem2_modulate(burst, burst_len, &modulated, &modulated_len, mod);
  TEST_ASSERT(modulated != NULL);
  for (size_t i = 0; i < modulated_len; i++) {
    float phase = (float) (2.0 * M_PI * carrier_offset_hz * (double) i / (double) sample_rate + 1.3);
    signal[offset + i] = amplitude * modulated[i] * cexpf(I * phase) + noise_sample(sigma);
  }
  return offset + modulated_len;
}

// lowest bit error ratio of the burst anywhere in the output
static double burst_mismatch_ratio(const int8_t *output, size_t output_len, const uint8_t *burst, size_t burst_bits) {
  double best = 1.0;
  for (size_t offset = 0; offset + burst_bits <= output_len; offset++) {
    size_t mismatches = 0;
    for (size_t i = 0; i < burst_bits; i++) {
      unsigned int expected = (burst[i / 8] >> (7 - (i % 8))) & 1U;
      unsigned int actual = output[offset + i] >= 0 ? 1U : 0U;
      if (expected != actual) {
        mismatches++;
      }
    }
    double ratio = (double) mismatches / (double) burst_bits;
    if (ratio < best) {
      best = ratio;
    }
  }
  return best;
}

// bursts separated by noise, each with its own carrier offset, amplitude, timing and polarity:
// every one of them is estimated from its own sync word
void test_bursts() {
  uint64_t sample_rate = 44100;
  GfskModemSettings settings = default_settings(sample_rate, BAUD_RATE / 4);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, INPUT_LEN, &mod));
  size_t chunk_len = 4096;
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, (uint32_t) chunk_len, &demod));

  setup_random_input(2 * INPUT_LEN);
  uint8_t *first = mod_input;
  uint8_t *second = mod_input + INPUT_LEN;
  memcpy(first, &SYNC_WORD, sizeof(SYNC_WORD));
  memcpy(second, SYNC_WORD_INVERTED, sizeof(SYNC_WORD_INVERTED));

  float sigma = 0.05f;
  size_t signal_len = 60000;
  float complex *signal = malloc(sizeof(float complex) * signal_len);
  TEST_ASSERT_NOT_NULL(signal);
  size_t len = append_noise(signal, 0, 1000, sigma);
  len = append_burst(first, INPUT_LEN, 300.0, 1.0f, sigma, sample_rate, signal, len);
  len = append_noise(signal, len, 3001, sigma);
  len = append_burst(second, INPUT_LEN, -500.0, 0.5f, sigma, sample_rate, signal, len);
  len = append_noise(signal, len, 3000, sigma);
  TEST_ASSERT(len <= signal_len);

  size_t output_capacity = len / (sample_rate / BAUD_RATE) + 64;
  int8_t *output = malloc(output_capacity);
  TEST_ASSERT_NOT_NULL(output);
  size_t output_len = 0;
  for (size_t i = 0; i < len; i += chunk_len) {
    size_t current = len - i < chunk_len ? len - i : chunk_len;
    int8_t *demodulated = NULL;
    size_t demodulated_len = 0;
    gfsk_modem2_demodulate(signal + i, current, &demodulated, &demodulated_len, demod);
    TEST_ASSERT(output_len + demodulated_len <= output_capacity);
    memcpy(output + output_len, demodulated, demodulated_len);
    output_len += demodulated_len;
  }
  free(signal);

  // the last symbols of a burst are still in the tx filter when the next modulate call starts
  size_t burst_bits = (INPUT_LEN - 1) * 8;
  double first_ratio = burst_mismatch_ratio(output, output_len, first, burst_bits);
  double second_ratio = burst_mismatch_ratio(output, output_len, second, burst_bits);
  free(output);
  TEST_ASSERT_TRUE_MESSAGE(first_ratio <= MAX_MISMATCH_RATIO, "first burst not recovered");
  TEST_ASSERT_TRUE_MESSAGE(second_ratio <= MAX_MISMATCH_RATIO, "second burst not recovered");
}

// modulated tone frequency must match the configured deviation: a stream of 1 bits at +deviation
void test_modulation_deviation() {
  GfskModemSettings settings = default_settings(8 * BAUD_RATE, 3000);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, 64, &mod));
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
  TEST_ASSERT_EQUAL_INT(-EINVAL, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, sample_rate, INPUT_LEN, &created));
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

void test_create_without_syncword() {
  GfskModemSettings settings = default_settings(48000, 2400);
  TEST_ASSERT_EQUAL_INT(-EINVAL, gfsk_modem2_create(&settings, 0, 0, settings.sample_rate, INPUT_LEN, &mod));
  TEST_ASSERT_NULL(mod);
  TEST_ASSERT_EQUAL_INT(-EINVAL, gfsk_modem2_create(&settings, SYNC_WORD, 0, settings.sample_rate, INPUT_LEN, &mod));
  TEST_ASSERT_NULL(mod);
}

void test_create_smallest_sps() {
  GfskModemSettings settings = default_settings(2 * BAUD_RATE, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, INPUT_LEN, &mod));
}

// rx runs at a different (decimated) rate than the raw tx one
void test_create_rx_rate_differs_from_tx() {
  GfskModemSettings settings = default_settings(192000, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, 24000, INPUT_LEN, &demod));
}

void test_invalid_buffers() {
  GfskModemSettings settings = default_settings(44100, 2400);
  TEST_ASSERT_EQUAL_INT(0, gfsk_modem2_create(&settings, SYNC_WORD, SYNC_WORD_BITS, settings.sample_rate, INPUT_LEN, &mod));
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
  RUN_TEST(test_soft_symbols_normalized_by_sync_word);
  RUN_TEST(test_bursts);
  RUN_TEST(test_modulation_deviation);
  RUN_TEST(test_create_invalid_settings);
  RUN_TEST(test_create_without_syncword);
  RUN_TEST(test_create_smallest_sps);
  RUN_TEST(test_create_rx_rate_differs_from_tx);
  RUN_TEST(test_invalid_buffers);
  return UNITY_END();
}
