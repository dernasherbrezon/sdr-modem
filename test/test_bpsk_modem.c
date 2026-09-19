#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unity.h>
#include "../src/dsp/bpsk_modem.h"
#include "utils.h"

#define BAUD_RATE 4800
// lowpass cutoff (one-sided). RRC with beta=0.35 occupies +-baud*(1+beta)/2 = +-3240Hz, so this
// leaves some margin and still is below nyquist for every sample rate used below
#define LOWPASS_BANDWIDTH 4800
#define INPUT_LEN 128
// the tx/rx pipelines add a few symbols of delay (RRC filters, resamplers, lowpass, symsync).
// exact value depends on the configuration, so search for it instead of hardcoding
#define MAX_BIT_LAG 40
// let the AGC, symbol timing and carrier loops settle. the slowest configuration here (resampled
// 44100/4800, sps 9) needs ~250 symbols before the output is clean
#define SKIP_BITS 300
// noiseless loopback: everything after the loops have settled should be recovered
#define MAX_MISMATCH_RATIO 0.02

bpsk_modem *mod = NULL;
bpsk_modem *demod = NULL;
uint8_t *mod_input = NULL;

static bpsk_modem_settings default_settings(uint64_t sample_rate, uint32_t bandwidth, psk_modem_type type) {
  bpsk_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = BAUD_RATE;
  settings.bandwidth = bandwidth;
  settings.rrc_beta = 0.35f;
  settings.rrc_delay = 5;
  settings.costas_bandwidth = 0.05f;
  settings.symsync_filter_bank_size = 32;
  settings.type = type;
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
static double mismatch_ratio(const int8_t *output, size_t output_len, size_t lag, bool inverted) {
  size_t total = 0;
  size_t mismatches = 0;
  for (size_t i = SKIP_BITS; i < output_len; i++) {
    size_t expected_index = i - lag;
    if (expected_index >= INPUT_LEN * 8) {
      break;
    }
    unsigned int expected_bit = input_bit(expected_index);
    unsigned int actual_bit = output[i] >= 0 ? 1U : 0U;
    if (inverted) {
      actual_bit ^= 1U;
    }
    if (actual_bit != expected_bit) {
      mismatches++;
    }
    total++;
  }
  TEST_ASSERT_GREATER_THAN_size_t(50, total);
  return (double) mismatches / (double) total;
}

// best (lowest) mismatch ratio over all bit lags and both polarities
static double best_mismatch_ratio(const int8_t *output, size_t output_len, bool *best_inverted) {
  double best = 1.0;
  *best_inverted = false;
  for (size_t lag = 0; lag <= MAX_BIT_LAG; lag++) {
    double normal = mismatch_ratio(output, output_len, lag, false);
    double inverted = mismatch_ratio(output, output_len, lag, true);
    if (normal < best) {
      best = normal;
      *best_inverted = false;
    }
    if (inverted < best) {
      best = inverted;
      *best_inverted = true;
    }
  }
  return best;
}

static void round_trip(uint64_t sample_rate, uint32_t bandwidth, psk_modem_type type, double carrier_offset_hz) {
  bpsk_modem_settings settings = default_settings(sample_rate, bandwidth, type);

  int code = bpsk_modem_create(&settings, INPUT_LEN, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  // demodulator has to accept everything the modulator can produce
  uint32_t max_samples = (uint32_t) bpsk_modem_max_modulation_buffer_length(mod);
  code = bpsk_modem_create(&settings, max_samples, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_random_input(INPUT_LEN);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &modulated, &modulated_len, mod);
  TEST_ASSERT(modulated != NULL);
  TEST_ASSERT(modulated_len > 0);
  TEST_ASSERT(modulated_len <= max_samples);
  // roughly sample_rate / baud_rate samples per bit. resampler and its filter can be off by a
  // few samples
  uint64_t expected_len = (uint64_t) INPUT_LEN * 8 * sample_rate / BAUD_RATE;
  TEST_ASSERT_UINT64_WITHIN(expected_len / 50 + 32, expected_len, modulated_len);

  if (carrier_offset_hz != 0.0) {
    // residual carrier error, i.e. SDR and transmitter clocks are not perfectly in sync. costas loop
    // has to track it without slipping onto a false lock
    for (size_t i = 0; i < modulated_len; i++) {
      modulated[i] *= cexpf(I * (float) (2.0 * M_PI * carrier_offset_hz * (double) i / (double) sample_rate + 0.7));
    }
  }

  int8_t *output = NULL;
  size_t output_len = 0;
  bpsk_modem_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT(output != NULL);
  TEST_ASSERT_GREATER_THAN_size_t(SKIP_BITS + 100, output_len);
  // can't recover more symbols than were sent
  TEST_ASSERT(output_len <= INPUT_LEN * 8 + 1);

  bool best_inverted = false;
  double best = best_mismatch_ratio(output, output_len, &best_inverted);

  if (type != BPSK) {
    // differential encoding is insensitive to the carrier phase ambiguity. inverted bits would
    // mean the modulator and demodulator disagree on the bit <-> phase-shift mapping
    TEST_ASSERT_FALSE_MESSAGE(best_inverted && best <= MAX_MISMATCH_RATIO, "differential bits are inverted");
  }
  // BPSK: costas loop can lock 180 degrees off, so either polarity is fine
  TEST_ASSERT_TRUE_MESSAGE(best <= MAX_MISMATCH_RATIO, "too many bit errors");
}

// tests are named <sample rate case>_<type>_<lowpass or none>
#define ROUND_TRIP_TESTS(name, sample_rate)                                                        \
  void test_##name##_bpsk_no_lowpass() { round_trip(sample_rate, 0, BPSK, 0); }                        \
  void test_##name##_bpsk_lowpass() { round_trip(sample_rate, LOWPASS_BANDWIDTH, BPSK, 0); }           \
  void test_##name##_sdpsk_no_lowpass() { round_trip(sample_rate, 0, SDPSK, 0); }                      \
  void test_##name##_sdpsk_lowpass() { round_trip(sample_rate, LOWPASS_BANDWIDTH, SDPSK, 0); }     \
  void test_##name##_dpsk_no_lowpass() { round_trip(sample_rate, 0, DPSK, 0); }                        \
  void test_##name##_dpsk_lowpass() { round_trip(sample_rate, LOWPASS_BANDWIDTH, DPSK, 0); }

#define RUN_ROUND_TRIP_TESTS(name)                                                                 \
  RUN_TEST(test_##name##_bpsk_no_lowpass);                                                          \
  RUN_TEST(test_##name##_bpsk_lowpass);                                                             \
  RUN_TEST(test_##name##_sdpsk_no_lowpass);                                                         \
  RUN_TEST(test_##name##_sdpsk_lowpass);                                                            \
  RUN_TEST(test_##name##_dpsk_no_lowpass);                                                          \
  RUN_TEST(test_##name##_dpsk_lowpass)

// sample rate is an exact multiple of the baud rate and sps is less than the internal target (15):
// no resampling
ROUND_TRIP_TESTS(exact_sps10, 48000)
// exact multiple, but sps (5) is far below the target: still no resampling
ROUND_TRIP_TESTS(exact_sps5, 24000)
// exact multiple above the target (20 > 15): input is resampled down to 15 sps
ROUND_TRIP_TESTS(exact_sps20, 96000)
// fractional sps (9.19) less than the target: resampled to the nearest integer sps (9)
ROUND_TRIP_TESTS(fractional_sps9, 44100)
// fractional sps (20.83) above the target: resampled to 15 sps
ROUND_TRIP_TESTS(fractional_sps20, 100000)

// residual carrier error must be tracked without the costas loop slipping onto a false lock.
// SDPSK is the most sensitive: a 4th-power detector has valid lock points at frequency offsets of
// +-90 and 180 degrees/symbol, where the bits come out random or inverted
void test_sdpsk_lowpass_carrier_offset() { round_trip(48000, LOWPASS_BANDWIDTH, SDPSK, 150.0); }
void test_sdpsk_lowpass_carrier_offset_fractional() { round_trip(44100, LOWPASS_BANDWIDTH, SDPSK, -150.0); }
void test_bpsk_carrier_offset() { round_trip(48000, LOWPASS_BANDWIDTH, BPSK, 150.0); }
void test_bpsk_lowpass_carrier_offset_resampled() { round_trip(100000, 6000, BPSK, 50.0); }
void test_dpsk_carrier_offset() { round_trip(48000, LOWPASS_BANDWIDTH, DPSK, -150.0); }

// edge cases: settings ///////////////////////////////////////////////////////////////////////////

static void assert_create_fails(uint64_t sample_rate, uint32_t baud_rate, uint32_t bandwidth) {
  bpsk_modem_settings settings = default_settings(sample_rate, bandwidth, BPSK);
  settings.baud_rate = baud_rate;
  bpsk_modem *created = NULL;
  TEST_ASSERT_EQUAL_INT(-EINVAL, bpsk_modem_create(&settings, INPUT_LEN, &created));
  TEST_ASSERT_NULL(created);
}

static void assert_create_succeeds(uint64_t sample_rate, uint32_t bandwidth) {
  bpsk_modem_settings settings = default_settings(sample_rate, bandwidth, BPSK);
  bpsk_modem *created = NULL;
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_create(&settings, INPUT_LEN, &created));
  TEST_ASSERT_NOT_NULL(created);
  bpsk_modem_destroy(created);
}

// samples per symbol is sample_rate / baud_rate rounded down and must be at least 2
void test_create_sps_too_small() {
  uint64_t sample_rates[] = {0, 1, BAUD_RATE / 2, BAUD_RATE, BAUD_RATE + 1, 7000, 2 * BAUD_RATE - 1};
  for (size_t i = 0; i < sizeof(sample_rates) / sizeof(sample_rates[0]); i++) {
    assert_create_fails(sample_rates[i], BAUD_RATE, 0);
    // sps is validated before the lowpass filter, so the bandwidth doesn't matter
    assert_create_fails(sample_rates[i], BAUD_RATE, LOWPASS_BANDWIDTH);
  }
}

void test_create_sps_smallest_allowed() {
  // exactly 2 sps: no resampling
  assert_create_succeeds(2 * BAUD_RATE, 0);
  // 2.99 sps rounds down to 2: resampled to the nearest integer sps
  assert_create_succeeds(3 * BAUD_RATE - 1, 0);
  // the lowpass cutoff has to be below nyquist of the (small) internal rate
  assert_create_succeeds(2 * BAUD_RATE, BAUD_RATE / 2);
}

void test_create_zero_baud_rate() {
  assert_create_fails(48000, 0, 0);
  assert_create_fails(48000, 0, LOWPASS_BANDWIDTH);
}

// lowpass cutoff is checked against nyquist of the internal sample rate (sps * baud_rate), not
// against the rate of the incoming samples
void test_create_lowpass_too_wide() {
  // sps 10, internal rate 48000
  assert_create_succeeds(48000, 23999);
  assert_create_fails(48000, BAUD_RATE, 24000);
  assert_create_fails(48000, BAUD_RATE, 24001);
  assert_create_fails(48000, BAUD_RATE, UINT32_MAX);
  // sample rate is above the target sps (15): internal rate is 72000, not 100000
  assert_create_succeeds(100000, 35999);
  assert_create_fails(100000, BAUD_RATE, 36000);
  assert_create_fails(100000, BAUD_RATE, 40000);
}

// there is no lower bound on the lowpass cutoff: even an absurdly narrow filter is accepted and
// must not break the pipeline (AGC amplifying a near-zero signal, etc)
void test_create_lowpass_very_narrow() {
  assert_create_succeeds(48000, 1);
  assert_create_succeeds(48000, 10);
  assert_create_succeeds(100000, 1);
}

// the filter is much narrower than the signal (RRC occupies +-3240Hz), so it removes nearly all of
// it. the modem must keep running and produce one decision per symbol, but the data is lost
static void narrow_lowpass_loses_data(psk_modem_type type, uint32_t bandwidth) {
  bpsk_modem_settings settings = default_settings(48000, bandwidth, type);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_create(&settings, INPUT_LEN, &mod));
  uint32_t max_samples = (uint32_t) bpsk_modem_max_modulation_buffer_length(mod);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_create(&settings, max_samples, &demod));
  setup_random_input(INPUT_LEN);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &modulated, &modulated_len, mod);
  TEST_ASSERT_NOT_NULL(modulated);

  int8_t *output = NULL;
  size_t output_len = 0;
  bpsk_modem_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(SKIP_BITS + 100, output_len);
  // the timing loop is starved by the missing signal, so the symbol count may drift a little away
  // from the number of bits sent. it still must fit into the output buffer
  TEST_ASSERT(output_len <= max_samples);
  TEST_ASSERT(output_len <= modulated_len);

  bool inverted = false;
  double best = best_mismatch_ratio(output, output_len, &inverted);
  // a working link is below MAX_MISMATCH_RATIO. random output would be ~0.5, even after picking
  // the best of all lags and polarities
  TEST_ASSERT_TRUE_MESSAGE(best > 0.2, "data was recovered through a lowpass filter that should have removed it");
}

void test_lowpass_too_narrow_bpsk() { narrow_lowpass_loses_data(BPSK, 100); }
void test_lowpass_too_narrow_sdpsk() { narrow_lowpass_loses_data(SDPSK, 100); }
void test_lowpass_too_narrow_dpsk() { narrow_lowpass_loses_data(DPSK, 100); }
void test_lowpass_narrowest() { narrow_lowpass_loses_data(BPSK, 1); }

// edge cases: buffers ////////////////////////////////////////////////////////////////////////////

// both directions have to work when the sample rate is an exact multiple of the baud rate
// (no resampler) and when it is not (resampler in front of the rx chain and behind the tx chain)
static void create_pair(uint64_t sample_rate, uint32_t bandwidth) {
  bpsk_modem_settings settings = default_settings(sample_rate, bandwidth, BPSK);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_create(&settings, INPUT_LEN, &mod));
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_create(&settings, INPUT_LEN, &demod));
}

static void demodulate_invalid_buffers(uint64_t sample_rate, uint32_t bandwidth) {
  create_pair(sample_rate, bandwidth);
  float complex *input = calloc(INPUT_LEN + 1, sizeof(float complex));
  TEST_ASSERT_NOT_NULL(input);

  // more samples than the modem was created for: rejected, output is reset so callers can't use
  // stale data
  int8_t sentinel = 0;
  int8_t *output = &sentinel;
  size_t output_len = 99;
  bpsk_modem_demodulate(input, INPUT_LEN + 1, &output, &output_len, demod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  output = &sentinel;
  output_len = 99;
  bpsk_modem_demodulate(input, SIZE_MAX, &output, &output_len, demod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // empty buffer is valid: nothing to decide on
  output = NULL;
  output_len = 99;
  bpsk_modem_demodulate(input, 0, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // exactly the max: accepted. can't produce more symbols than samples
  output = NULL;
  output_len = 0;
  bpsk_modem_demodulate(input, INPUT_LEN, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT(output_len <= INPUT_LEN);

  // rejected buffers must not have corrupted the modem
  output = NULL;
  output_len = 0;
  bpsk_modem_demodulate(input, INPUT_LEN, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  free(input);
}

void test_demodulate_invalid_buffers() { demodulate_invalid_buffers(48000, 0); }
void test_demodulate_invalid_buffers_lowpass() { demodulate_invalid_buffers(48000, LOWPASS_BANDWIDTH); }
void test_demodulate_invalid_buffers_resampled() { demodulate_invalid_buffers(44100, LOWPASS_BANDWIDTH); }

static void modulate_invalid_buffers(uint64_t sample_rate, uint32_t bandwidth) {
  create_pair(sample_rate, bandwidth);
  // one byte more than the maximum
  setup_random_input(INPUT_LEN + 1);
  size_t max_samples = bpsk_modem_max_modulation_buffer_length(mod);

  float complex sentinel = 0;
  float complex *output = &sentinel;
  size_t output_len = 99;
  bpsk_modem_modulate(mod_input, INPUT_LEN + 1, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // input_len * 8 wraps around for these, but lands on a huge value (2^64 - 8) and is still rejected
  output = &sentinel;
  output_len = 99;
  bpsk_modem_modulate(mod_input, SIZE_MAX, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  output = &sentinel;
  output_len = 99;
  bpsk_modem_modulate(mod_input, SIZE_MAX / 2, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // empty buffer is valid: no symbols, no samples
  output = NULL;
  output_len = 99;
  bpsk_modem_modulate(mod_input, 0, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // exactly the max: accepted and fits into the advertised output buffer
  output = NULL;
  output_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
  TEST_ASSERT(output_len <= max_samples);

  // single byte
  output = NULL;
  output_len = 0;
  bpsk_modem_modulate(mod_input, 1, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
}

void test_modulate_invalid_buffers() { modulate_invalid_buffers(48000, 0); }
void test_modulate_invalid_buffers_resampled() { modulate_invalid_buffers(44100, 0); }

// debug constellation file /////////////////////////////////////////////////////////////////////////

#define DEBUG_FILE "test_bpsk_modem_constellation.bin"

static long file_size(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long result = ftell(f);
  fclose(f);
  return result;
}

// tx dumps one constellation point per transmitted bit
void test_debug_constellation_modulate() {
  create_pair(48000, 0);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_set_debug_constellation_file(mod, DEBUG_FILE));
  setup_random_input(INPUT_LEN);
  float complex *output = NULL;
  size_t output_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &output, &output_len, mod);
  bpsk_modem_destroy(mod);
  mod = NULL;
  long size = file_size(DEBUG_FILE);
  remove(DEBUG_FILE);
  TEST_ASSERT_EQUAL_INT64((long) (INPUT_LEN * 8 * sizeof(float complex)), size);
}

// rx dumps one constellation point per recovered symbol
void test_debug_constellation_demodulate() {
  create_pair(48000, 0);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_set_debug_constellation_file(demod, DEBUG_FILE));
  float complex *input = calloc(INPUT_LEN, sizeof(float complex));
  TEST_ASSERT_NOT_NULL(input);
  int8_t *output = NULL;
  size_t output_len = 0;
  bpsk_modem_demodulate(input, INPUT_LEN, &output, &output_len, demod);
  free(input);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
  bpsk_modem_destroy(demod);
  demod = NULL;
  long size = file_size(DEBUG_FILE);
  remove(DEBUG_FILE);
  TEST_ASSERT_EQUAL_INT64((long) (output_len * sizeof(float complex)), size);
}

void test_debug_constellation_disabled_with_null() {
  create_pair(48000, 0);
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_set_debug_constellation_file(mod, DEBUG_FILE));
  TEST_ASSERT_EQUAL_INT(0, bpsk_modem_set_debug_constellation_file(mod, NULL));
  setup_random_input(INPUT_LEN);
  float complex *output = NULL;
  size_t output_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  bpsk_modem_destroy(mod);
  mod = NULL;
  long size = file_size(DEBUG_FILE);
  remove(DEBUG_FILE);
  // the file was created, but nothing was written
  TEST_ASSERT_EQUAL_INT64(0, size);
}

void test_debug_constellation_invalid_path() {
  create_pair(48000, 0);
  TEST_ASSERT_NOT_EQUAL_INT(0, bpsk_modem_set_debug_constellation_file(mod, "/nonexistent-directory/constellation.bin"));
  // modem is still usable without the dump
  setup_random_input(INPUT_LEN);
  float complex *output = NULL;
  size_t output_len = 0;
  bpsk_modem_modulate(mod_input, INPUT_LEN, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
}

void tearDown() {
  if (mod != NULL) {
    bpsk_modem_destroy(mod);
    mod = NULL;
  }
  if (demod != NULL) {
    bpsk_modem_destroy(demod);
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
  RUN_ROUND_TRIP_TESTS(exact_sps10);
  RUN_ROUND_TRIP_TESTS(exact_sps5);
  RUN_ROUND_TRIP_TESTS(exact_sps20);
  RUN_ROUND_TRIP_TESTS(fractional_sps9);
  RUN_ROUND_TRIP_TESTS(fractional_sps20);
  RUN_TEST(test_sdpsk_lowpass_carrier_offset);
  RUN_TEST(test_sdpsk_lowpass_carrier_offset_fractional);
  RUN_TEST(test_bpsk_carrier_offset);
  RUN_TEST(test_bpsk_lowpass_carrier_offset_resampled);
  RUN_TEST(test_dpsk_carrier_offset);
  RUN_TEST(test_create_sps_too_small);
  RUN_TEST(test_create_sps_smallest_allowed);
  RUN_TEST(test_create_zero_baud_rate);
  RUN_TEST(test_create_lowpass_too_wide);
  RUN_TEST(test_create_lowpass_very_narrow);
  RUN_TEST(test_lowpass_too_narrow_bpsk);
  RUN_TEST(test_lowpass_too_narrow_sdpsk);
  RUN_TEST(test_lowpass_too_narrow_dpsk);
  RUN_TEST(test_lowpass_narrowest);
  RUN_TEST(test_demodulate_invalid_buffers);
  RUN_TEST(test_demodulate_invalid_buffers_lowpass);
  RUN_TEST(test_demodulate_invalid_buffers_resampled);
  RUN_TEST(test_modulate_invalid_buffers);
  RUN_TEST(test_modulate_invalid_buffers_resampled);
  RUN_TEST(test_debug_constellation_modulate);
  RUN_TEST(test_debug_constellation_demodulate);
  RUN_TEST(test_debug_constellation_disabled_with_null);
  RUN_TEST(test_debug_constellation_invalid_path);
  return UNITY_END();
}
