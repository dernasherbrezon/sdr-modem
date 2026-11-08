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
  return UNITY_END();
}
