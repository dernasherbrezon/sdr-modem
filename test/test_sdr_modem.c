#include <stdlib.h>
#include <string.h>
#include <unity.h>
#include "../src/dsp/sdr_modem.h"
#include "utils.h"

#define BAUD_RATE 4800
#define INPUT_LEN 32

sdr_modem *modem = NULL;
uint8_t *input = NULL;

static sdr_modem_settings gfsk_settings(uint64_t sample_rate) {
  sdr_modem_settings settings = {0};
  settings.modem_type = MODEM_TYPE_GFSK;
  settings.modem.gfsk.sample_rate = sample_rate;
  settings.modem.gfsk.baud_rate = BAUD_RATE;
  settings.modem.gfsk.deviation = 3000;
  settings.modem.gfsk.bandwidth = 12000;
  settings.modem.gfsk.bt = 0.5f;
  return settings;
}

static size_t modulate_and_flush(size_t chunk_len, float complex *result, size_t capacity) {
  size_t total = 0;
  for (size_t offset = 0; offset < INPUT_LEN; offset += chunk_len) {
    float complex *output = NULL;
    size_t output_len = 0;
    sdr_modem_modulate(input + offset, chunk_len, &output, &output_len, modem);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT(total + output_len <= capacity);
    memcpy(result + total, output, sizeof(float complex) * output_len);
    total += output_len;
  }
  float complex *output = NULL;
  size_t output_len = 0;
  sdr_modem_modulate_flush(&output, &output_len, modem);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT(total + output_len <= capacity);
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

// 1.92 MHz is far above the signal bandwidth: the modem runs behind the half-band interpolator
static void create_with_halfband() {
  sdr_modem_settings settings = gfsk_settings(1920000);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_create(&settings, INPUT_LEN, NULL, &modem));
  setup_random_input(&input, INPUT_LEN);
}

void test_flush_without_modulate() {
  create_with_halfband();
  float complex *output = (float complex *) 1;
  size_t output_len = 123;
  sdr_modem_modulate_flush(&output, &output_len, modem);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
}

void test_modulate_has_no_tail() {
  create_with_halfband();
  float complex *output = NULL;
  size_t output_len = 0;
  sdr_modem_modulate(input, INPUT_LEN, &output, &output_len, modem);
  TEST_ASSERT_NOT_NULL(output);
  // the frame only: a tail would make it longer by the interpolator and pulse delay
  size_t expected = (size_t) INPUT_LEN * 8 * (1920000 / BAUD_RATE);
  TEST_ASSERT_UINT32_WITHIN(expected / 50, (uint32_t) expected, (uint32_t) output_len);
  sdr_modem_modulate_flush(&output, &output_len, modem);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_GREATER_THAN_size_t(0, output_len);
  // consumed: the second flush has nothing
  sdr_modem_modulate_flush(&output, &output_len, modem);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_size_t(0, output_len);
}

void test_flush_resets_state() {
  create_with_halfband();
  size_t capacity = sdr_modem_max_modulation_buffer_length(modem) * 2;
  float complex *first = malloc(sizeof(float complex) * capacity);
  float complex *second = malloc(sizeof(float complex) * capacity);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  size_t first_len = modulate_and_flush(INPUT_LEN, first, capacity);
  size_t second_len = modulate_and_flush(INPUT_LEN, second, capacity);
  assert_same_signal(first, first_len, second, second_len);
  free(first);
  free(second);
}

void test_streaming_chunks() {
  create_with_halfband();
  size_t capacity = sdr_modem_max_modulation_buffer_length(modem) * 2;
  float complex *whole = malloc(sizeof(float complex) * capacity);
  float complex *chunks = malloc(sizeof(float complex) * capacity);
  TEST_ASSERT_NOT_NULL(whole);
  TEST_ASSERT_NOT_NULL(chunks);
  size_t whole_len = modulate_and_flush(INPUT_LEN, whole, capacity);
  size_t chunks_len = modulate_and_flush(8, chunks, capacity);
  assert_same_signal(whole, whole_len, chunks, chunks_len);
  free(whole);
  free(chunks);
}

void test_frequency_offset_null() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sdr_modem_get_frequency_offset(NULL));
}

// the offset is reported in Hz of the original sample rate, behind the half-band decimator
void test_frequency_offset_bpsk() {
  sdr_modem_settings settings = {0};
  settings.modem_type = MODEM_TYPE_BPSK;
  settings.modem.psk.sample_rate = 48000;
  settings.modem.psk.baud_rate = BAUD_RATE;
  settings.modem.psk.rrc_beta = 0.35f;
  settings.modem.psk.rrc_delay = 5;
  settings.modem.psk.costas_bandwidth = 0.05f;
  settings.modem.psk.symsync_filter_bank_size = 32;
  settings.modem.psk.type = BPSK;
  size_t len = 128;
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_create(&settings, (uint32_t) len, NULL, &modem));
  size_t max_samples = sdr_modem_max_modulation_buffer_length(modem);
  sdr_modem *demod = NULL;
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_create(&settings, (uint32_t) max_samples, NULL, &demod));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, sdr_modem_get_frequency_offset(demod));

  setup_random_input(&input, len);
  float complex *modulated = NULL;
  size_t modulated_len = 0;
  sdr_modem_modulate(input, len, &modulated, &modulated_len, modem);
  TEST_ASSERT_NOT_NULL(modulated);
  const double offset_hz = 150.0;
  for (size_t i = 0; i < modulated_len; i++) {
    modulated[i] *= cexpf(I * (float) (2.0 * 3.14159265358979323846 * offset_hz * (double) i / 48000.0 + 0.7));
  }
  int8_t *output = NULL;
  size_t output_len = 0;
  sdr_modem_demodulate(modulated, modulated_len, &output, &output_len, demod);
  TEST_ASSERT_NOT_NULL(output);
  TEST_ASSERT_FLOAT_WITHIN(25.0f, (float) offset_hz, sdr_modem_get_frequency_offset(demod));
  sdr_modem_destroy(demod);
}

typedef struct {
  int count;
  float frequency;
} carrier_events;

static void on_carrier(float frequency, void *ctx) {
  carrier_events *events = (carrier_events *) ctx;
  events->count++;
  events->frequency = frequency;
}

static float carrier_test_noise(uint32_t *state) {
  float sum = 0.0f;
  for (int i = 0; i < 4; i++) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    sum += (float) (*state >> 8) / (float) (1 << 24) - 0.5f;
  }
  return sum;
}

// the carrier is detected behind the half-band decimator, before the demodulator, whatever the modem is.
// the frequency is reported in Hz of the original sample rate
void test_carrier_callback() {
  const uint32_t chunk_len = 4096;
  const uint64_t sample_rate = 1920000;
  sdr_modem_settings settings = gfsk_settings(sample_rate);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_create(&settings, chunk_len, NULL, &modem));
  carrier_events events = {0};
  sdr_modem_set_carrier_callback(on_carrier, &events, modem);

  float complex *chunk = malloc(sizeof(float complex) * chunk_len);
  TEST_ASSERT_NOT_NULL(chunk);
  uint32_t rng = 777;
  const float carrier_frequency = 2000.0f;
  const size_t noise_samples = 768000;
  const size_t carrier_samples = 768000;
  for (size_t offset = 0; offset < noise_samples + carrier_samples; offset += chunk_len) {
    for (size_t i = 0; i < chunk_len; i++) {
      size_t n = offset + i;
      float complex value = 0.5f * (carrier_test_noise(&rng) + I * carrier_test_noise(&rng));
      if (n >= noise_samples) {
        value += 0.5f * cexpf(I * (float) (2.0 * 3.14159265358979323846 * carrier_frequency * (double) n / (double) sample_rate));
      }
      chunk[i] = value;
    }
    int8_t *output = NULL;
    size_t output_len = 0;
    sdr_modem_demodulate(chunk, chunk_len, &output, &output_len, modem);
    if (offset + chunk_len <= noise_samples) {
      TEST_ASSERT_EQUAL_INT(0, events.count);
    }
  }
  free(chunk);
  TEST_ASSERT_EQUAL_INT(1, events.count);
  TEST_ASSERT_FLOAT_WITHIN(20.0f, carrier_frequency, events.frequency);
  // the offset which the modem reports includes the detected carrier
  TEST_ASSERT_FLOAT_WITHIN(40.0f, carrier_frequency, sdr_modem_get_frequency_offset(modem));
}

// the wrapped modem must get the carrier at the center: the dump of its input is mixed down by the detected frequency
void test_carrier_is_removed_before_modem() {
  const uint32_t chunk_len = 4096;
  const uint64_t sample_rate = 1920000;
  const double baseband_rate = 30000.0;
  const char *debug_file = "test_carrier_baseband.cf32";
  sdr_modem_settings settings = gfsk_settings(sample_rate);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_create(&settings, chunk_len, NULL, &modem));
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_set_debug_baseband_file(debug_file, modem));

  float complex *chunk = malloc(sizeof(float complex) * chunk_len);
  TEST_ASSERT_NOT_NULL(chunk);
  uint32_t rng = 4242;
  const float carrier_frequency = -2500.0f;
  const size_t noise_samples = 768000;
  const size_t total_samples = 2 * 768000;
  for (size_t offset = 0; offset < total_samples; offset += chunk_len) {
    for (size_t i = 0; i < chunk_len; i++) {
      size_t n = offset + i;
      float complex value = 0.5f * (carrier_test_noise(&rng) + I * carrier_test_noise(&rng));
      if (n >= noise_samples) {
        value += 0.5f * cexpf(I * (float) (2.0 * 3.14159265358979323846 * carrier_frequency * (double) n / (double) sample_rate));
      }
      chunk[i] = value;
    }
    int8_t *output = NULL;
    size_t output_len = 0;
    sdr_modem_demodulate(chunk, chunk_len, &output, &output_len, modem);
  }
  free(chunk);
  TEST_ASSERT_FLOAT_WITHIN(40.0f, carrier_frequency, sdr_modem_get_frequency_offset(modem));
  // flush the dump
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_set_debug_baseband_file(NULL, modem));

  FILE *file = fopen(debug_file, "rb");
  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
  long size = ftell(file);
  const size_t tail_len = 4096;
  TEST_ASSERT_TRUE(size >= (long) (tail_len * sizeof(float complex)));
  TEST_ASSERT_EQUAL_INT(0, fseek(file, size - (long) (tail_len * sizeof(float complex)), SEEK_SET));
  float complex *tail = malloc(sizeof(float complex) * tail_len);
  TEST_ASSERT_NOT_NULL(tail);
  TEST_ASSERT_EQUAL_size_t(tail_len, fread(tail, sizeof(float complex), tail_len, file));
  fclose(file);
  remove(debug_file);
  // mean frequency of the tail: lag-1 autocorrelation
  float complex sum = 0.0f;
  for (size_t i = 1; i < tail_len; i++) {
    sum += tail[i] * conjf(tail[i - 1]);
  }
  free(tail);
  double residual = (double) cargf(sum) * baseband_rate / (2.0 * 3.14159265358979323846);
  // without the correction it would be -2500 Hz
  TEST_ASSERT_FLOAT_WITHIN(40.0f, 0.0f, (float) residual);
}

void test_carrier_callback_null_modem() {
  sdr_modem_set_carrier_callback(on_carrier, NULL, NULL);
}

void tearDown() {
  if (modem != NULL) {
    sdr_modem_destroy(modem);
    modem = NULL;
  }
  if (input != NULL) {
    free(input);
    input = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_flush_without_modulate);
  RUN_TEST(test_modulate_has_no_tail);
  RUN_TEST(test_flush_resets_state);
  RUN_TEST(test_streaming_chunks);
  RUN_TEST(test_frequency_offset_null);
  RUN_TEST(test_frequency_offset_bpsk);
  RUN_TEST(test_carrier_callback);
  RUN_TEST(test_carrier_is_removed_before_modem);
  RUN_TEST(test_carrier_callback_null_modem);
  return UNITY_END();
}
