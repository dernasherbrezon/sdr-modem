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

static void round_trip(uint64_t sample_rate, uint32_t bandwidth, psk_modem_type type, double carrier_offset_hz) {
  bpsk_modem_settings settings = default_settings(sample_rate, bandwidth, type);

  int code = bpsk_modem_create(&settings, INPUT_LEN, NULL, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  // demodulator has to accept everything the modulator can produce
  uint32_t max_samples = (uint32_t) bpsk_modem_max_modulation_buffer_length(mod);
  code = bpsk_modem_create(&settings, max_samples, NULL, &demod);
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

  double best = 1.0;
  bool best_inverted = false;
  for (size_t lag = 0; lag <= MAX_BIT_LAG; lag++) {
    double normal = mismatch_ratio(output, output_len, lag, false);
    double inverted = mismatch_ratio(output, output_len, lag, true);
    if (normal < best) {
      best = normal;
      best_inverted = false;
    }
    if (inverted < best) {
      best = inverted;
      best_inverted = true;
    }
  }

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
  return UNITY_END();
}
