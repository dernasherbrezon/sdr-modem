#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unity.h>
#include "../src/dsp/oqpsk_modem.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// symbol rate. every symbol carries 2 bits: I and Q
#define BAUD_RATE 4800
#define INPUT_LEN 128
// the tx/rx pipelines add a few symbols of delay (RRC filters, resamplers, symbol clock). exact
// value depends on the configuration, so search for it instead of hardcoding
#define MAX_SYMBOL_LAG 40
// let the AGC, symbol timing and carrier loops settle
#define SKIP_SYMBOLS 150
// noiseless loopback: everything after the loops have settled should be recovered
#define MAX_MISMATCH_RATIO 0.02

oqpsk_modem *mod = NULL;
oqpsk_modem *demod = NULL;
uint8_t *mod_input = NULL;
int8_t *rx_bits = NULL;

static oqpsk_modem_settings default_settings(uint64_t sample_rate) {
  oqpsk_modem_settings settings = {0};
  settings.sample_rate = sample_rate;
  settings.baud_rate = BAUD_RATE;
  settings.rrc_beta = 0.35f;
  settings.rrc_delay = 5;
  settings.costas_bandwidth = 0.05f;
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

// The bit expected at the position i of the demodulated stream (I, Q interleaved, MSB first, like
// the modulator consumes it) if the stream is lag symbols late and the constellation is rotated by
// rotation * 90 degrees
static unsigned int expected_bit(size_t i, size_t lag, unsigned int rotation) {
  size_t symbol = i / 2 - lag;
  unsigned int in_i = input_bit(2 * symbol);
  unsigned int in_q = input_bit(2 * symbol + 1);
  unsigned int out_i;
  unsigned int out_q;
  switch (rotation) {
    case 1:
      // multiply by j: (I, Q) -> (-Q, I)
      out_i = in_q ^ 1U;
      out_q = in_i;
      break;
    case 2:
      out_i = in_i ^ 1U;
      out_q = in_q ^ 1U;
      break;
    case 3:
      // multiply by -j: (I, Q) -> (Q, -I)
      out_i = in_q;
      out_q = in_i ^ 1U;
      break;
    default:
      out_i = in_i;
      out_q = in_q;
      break;
  }
  return (i % 2 == 0) ? out_i : out_q;
}

// fraction of demodulated hard decisions that differ from the input bits
static double mismatch_ratio(const int8_t *output, size_t output_len, size_t lag, unsigned int rotation) {
  size_t total = 0;
  size_t mismatches = 0;
  for (size_t i = SKIP_SYMBOLS * 2; i < output_len; i++) {
    if (i / 2 - lag >= INPUT_LEN * 4) {
      break;
    }
    unsigned int actual_bit = output[i] >= 0 ? 1U : 0U;
    if (actual_bit != expected_bit(i, lag, rotation)) {
      mismatches++;
    }
    total++;
  }
  TEST_ASSERT_GREATER_THAN_size_t(100, total);
  return (double) mismatches / (double) total;
}

// best (lowest) mismatch ratio over all symbol lags and constellation rotations. carrier recovery
// of a QPSK-like constellation has a 4-fold phase ambiguity, so every rotation is a valid lock
static double best_mismatch_ratio(const int8_t *output, size_t output_len) {
  double best = 1.0;
  for (size_t lag = 0; lag <= MAX_SYMBOL_LAG; lag++) {
    for (unsigned int rotation = 0; rotation < 4; rotation++) {
      double ratio = mismatch_ratio(output, output_len, lag, rotation);
      if (ratio < best) {
        best = ratio;
      }
    }
  }
  return best;
}

// demod_chunk is the number of samples given to the demodulator at once. 0 means the whole signal
static void round_trip(uint64_t sample_rate, double carrier_offset_hz, double phase_offset_rad, size_t demod_chunk) {
  oqpsk_modem_settings settings = default_settings(sample_rate);

  int code = oqpsk_modem_create(&settings, INPUT_LEN, &mod);
  TEST_ASSERT_EQUAL_INT(0, code);
  // demodulator has to accept everything the modulator can produce
  uint32_t max_samples = (uint32_t) oqpsk_modem_max_modulation_buffer_length(mod);
  code = oqpsk_modem_create(&settings, max_samples, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_random_input(INPUT_LEN);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  oqpsk_modem_modulate(mod_input, INPUT_LEN, &modulated, &modulated_len, mod);
  TEST_ASSERT(modulated != NULL);
  TEST_ASSERT(modulated_len > 0);
  TEST_ASSERT(modulated_len <= max_samples);
  // 2 bits per symbol: roughly sample_rate / baud_rate samples per 2 bits. resampler and its filter
  // can be off by a few samples
  uint64_t expected_len = (uint64_t) INPUT_LEN * 4 * sample_rate / BAUD_RATE;
  TEST_ASSERT_UINT64_WITHIN(expected_len / 50 + 32, expected_len, modulated_len);

  if (carrier_offset_hz != 0.0 || phase_offset_rad != 0.0) {
    for (size_t i = 0; i < modulated_len; i++) {
      modulated[i] *= cexpf(I * (float) (2.0 * M_PI * carrier_offset_hz * (double) i / (double) sample_rate + phase_offset_rad));
    }
  }

  size_t chunk = demod_chunk == 0 ? modulated_len : demod_chunk;
  rx_bits = malloc(sizeof(int8_t) * (INPUT_LEN * 8 + 16));
  TEST_ASSERT_NOT_NULL(rx_bits);
  size_t rx_len = 0;
  for (size_t offset = 0; offset < modulated_len; offset += chunk) {
    size_t len = modulated_len - offset < chunk ? modulated_len - offset : chunk;
    int8_t *output = NULL;
    size_t output_len = 0;
    oqpsk_modem_demodulate(modulated + offset, len, &output, &output_len, demod);
    TEST_ASSERT(output != NULL);
    // one soft bit per I and per Q of every recovered symbol
    TEST_ASSERT_EQUAL_size_t(0, output_len % 2);
    // can't recover more symbols than were sent
    TEST_ASSERT(rx_len + output_len <= INPUT_LEN * 8 + 16);
    memcpy(rx_bits + rx_len, output, output_len);
    rx_len += output_len;
  }
  TEST_ASSERT_GREATER_THAN_size_t(SKIP_SYMBOLS * 2 + 200, rx_len);

  double best = best_mismatch_ratio(rx_bits, rx_len);
  TEST_ASSERT_TRUE_MESSAGE(best <= MAX_MISMATCH_RATIO, "too many bit errors");
}

// tests are named <sample rate case>[_<phase offset>|_chunked]

// sample rate is an exact multiple of the baud rate: no resampling
void test_exact_sps10() { round_trip(48000, 0, 0, 0); }
void test_exact_sps5() { round_trip(24000, 0, 0, 0); }
void test_exact_sps20() { round_trip(96000, 0, 0, 0); }
// odd sps: the half symbol delay of the Q rail can't be exact
void test_exact_sps9() { round_trip(43200, 0, 0, 0); }
// fractional sps (9.19): resampled to the nearest integer sps (9)
void test_fractional_sps9() { round_trip(44100, 0, 0, 0); }
// fractional sps (20.83): resampled to the nearest integer sps (21)
void test_fractional_sps21() { round_trip(100000, 0, 0, 0); }

// the demodulator is fed with a stream of small buffers, like a real SDR does. AGC, symbol clock and
// carrier loop state has to be carried over between calls
void test_chunked() { round_trip(48000, 0, 0, 1000); }
void test_chunked_odd_size() { round_trip(48000, 0, 0, 777); }
void test_chunked_fractional() { round_trip(44100, 0, 0, 1000); }

// constant carrier phase error, i.e. the transmitter and the receiver oscillators are not in phase.
// costas loop has to remove it
//
// known limitation, not covered here: a frequency offset (even 50Hz) or a static phase offset close to
// 45 degrees (0.7 rad) is not tracked. symbol timing recovery works on the I rail alone and runs
// before the carrier recovery, so a rotated constellation leaks Q into I, the timing loop loses lock
// and the costas loop never gets clean symbols
void test_phase_offset() { round_trip(48000, 0, 0.3, 0); }
void test_phase_offset_negative() { round_trip(48000, 0, -0.3, 0); }
void test_phase_offset_fractional() { round_trip(44100, 0, 0.3, 0); }

// the constellation has to be locked on the diagonals (45, 135, 225 and 315 degrees), which is where
// the data of OQPSK is. a phase detector that is stable on the axes instead locks 45 degrees off,
// where the sign of each rail is random. best_mismatch_ratio() catches it too, this one points
// straight to the reason
void test_constellation_on_diagonals() {
  oqpsk_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_create(&settings, INPUT_LEN, &mod));
  uint32_t max_samples = (uint32_t) oqpsk_modem_max_modulation_buffer_length(mod);
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_create(&settings, max_samples, &demod));
  const char *path = "test_oqpsk_modem_constellation.bin";
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_set_debug_constellation_file(path, demod));
  setup_random_input(INPUT_LEN);

  float complex *modulated = NULL;
  size_t modulated_len = 0;
  oqpsk_modem_modulate(mod_input, INPUT_LEN, &modulated, &modulated_len, mod);
  TEST_ASSERT_NOT_NULL(modulated);
  int8_t *output = NULL;
  size_t output_len = 0;
  oqpsk_modem_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  size_t symbols = output_len / 2;
  TEST_ASSERT_GREATER_THAN_size_t(SKIP_SYMBOLS + 100, symbols);
  // flush the dump
  oqpsk_modem_destroy(demod);
  demod = NULL;

  FILE *file = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL(file);
  float complex *points = malloc(sizeof(float complex) * symbols);
  TEST_ASSERT_NOT_NULL(points);
  size_t actual_read = fread(points, sizeof(float complex), symbols, file);
  fclose(file);
  remove(path);
  TEST_ASSERT_EQUAL_size_t(symbols, actual_read);

  for (size_t i = SKIP_SYMBOLS; i < symbols; i++) {
    float magnitude = cabsf(points[i]);
    TEST_ASSERT(magnitude > 0.0f);
    // on a diagonal both rails have the same magnitude. on an axis one of them is ~0
    float ratio = fminf(fabsf(crealf(points[i])), fabsf(cimagf(points[i]))) / fmaxf(fabsf(crealf(points[i])), fabsf(cimagf(points[i])));
    TEST_ASSERT_TRUE_MESSAGE(ratio > 0.7f, "constellation is not on the diagonals");
  }
  free(points);
}

// edge cases: settings ///////////////////////////////////////////////////////////////////////////

static void assert_create_fails(uint64_t sample_rate, uint32_t baud_rate) {
  oqpsk_modem_settings settings = default_settings(sample_rate);
  settings.baud_rate = baud_rate;
  oqpsk_modem *created = NULL;
  TEST_ASSERT_EQUAL_INT(-EINVAL, oqpsk_modem_create(&settings, INPUT_LEN, &created));
  TEST_ASSERT_NULL(created);
}

// samples per symbol must be at least 2
void test_create_sps_too_small() {
  uint64_t sample_rates[] = {0, 1, BAUD_RATE / 2, BAUD_RATE, BAUD_RATE + 1, 7000};
  for (size_t i = 0; i < sizeof(sample_rates) / sizeof(sample_rates[0]); i++) {
    assert_create_fails(sample_rates[i], BAUD_RATE);
  }
}

void test_create_zero_baud_rate() {
  assert_create_fails(48000, 0);
}

void test_create_sps_smallest_allowed() {
  oqpsk_modem_settings settings = default_settings(2 * BAUD_RATE);
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_create(&settings, INPUT_LEN, &mod));
}

// edge cases: buffers ////////////////////////////////////////////////////////////////////////////

void test_demodulate_invalid_buffers() {
  oqpsk_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_create(&settings, INPUT_LEN, &demod));
  float complex *input = calloc(INPUT_LEN + 1, sizeof(float complex));
  TEST_ASSERT_NOT_NULL(input);

  // more samples than the modem was created for: rejected, output is reset so callers can't use
  // stale data
  int8_t sentinel = 0;
  int8_t *output = &sentinel;
  size_t output_len = 99;
  oqpsk_modem_demodulate(input, INPUT_LEN + 1, &output, &output_len, demod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  output = &sentinel;
  output_len = 99;
  oqpsk_modem_demodulate(input, SIZE_MAX, &output, &output_len, demod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // exactly the max: accepted
  output = NULL;
  output_len = 0;
  oqpsk_modem_demodulate(input, INPUT_LEN, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len % 2);
  free(input);
}

void test_modulate_invalid_buffers() {
  oqpsk_modem_settings settings = default_settings(48000);
  TEST_ASSERT_EQUAL_INT(0, oqpsk_modem_create(&settings, INPUT_LEN, &mod));
  // one byte more than the maximum
  setup_random_input(INPUT_LEN + 1);
  size_t max_samples = oqpsk_modem_max_modulation_buffer_length(mod);

  float complex sentinel = 0;
  float complex *output = &sentinel;
  size_t output_len = 99;
  oqpsk_modem_modulate(mod_input, INPUT_LEN + 1, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  output = &sentinel;
  output_len = 99;
  oqpsk_modem_modulate(mod_input, SIZE_MAX, &output, &output_len, mod);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);

  // exactly the max: accepted and fits into the advertised output buffer
  output = NULL;
  output_len = 0;
  oqpsk_modem_modulate(mod_input, INPUT_LEN, &output, &output_len, mod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
  TEST_ASSERT(output_len <= max_samples);
}

void tearDown() {
  if (mod != NULL) {
    oqpsk_modem_destroy(mod);
    mod = NULL;
  }
  if (demod != NULL) {
    oqpsk_modem_destroy(demod);
    demod = NULL;
  }
  if (mod_input != NULL) {
    free(mod_input);
    mod_input = NULL;
  }
  if (rx_bits != NULL) {
    free(rx_bits);
    rx_bits = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_exact_sps10);
  RUN_TEST(test_exact_sps5);
  RUN_TEST(test_exact_sps20);
  RUN_TEST(test_exact_sps9);
  RUN_TEST(test_fractional_sps9);
  RUN_TEST(test_fractional_sps21);
  RUN_TEST(test_chunked);
  RUN_TEST(test_chunked_odd_size);
  RUN_TEST(test_chunked_fractional);
  RUN_TEST(test_phase_offset);
  RUN_TEST(test_phase_offset_negative);
  RUN_TEST(test_phase_offset_fractional);
  RUN_TEST(test_constellation_on_diagonals);
  RUN_TEST(test_create_sps_too_small);
  RUN_TEST(test_create_zero_baud_rate);
  RUN_TEST(test_create_sps_smallest_allowed);
  RUN_TEST(test_demodulate_invalid_buffers);
  RUN_TEST(test_modulate_invalid_buffers);
  return UNITY_END();
}
