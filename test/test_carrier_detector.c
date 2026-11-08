#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>
#include "../src/dsp/carrier_detector.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 20000

carrier_detector *detector = NULL;
float complex *signal_buffer = NULL;
size_t signal_len = 0;
uint32_t rng_state = 12345;

typedef struct {
  // CARRIER_DETECTED events
  int count;
  float frequency;
  int lost_count;
} events;

static void on_carrier(carrier_detector_event_type event_type, float frequency, void *ctx) {
  events *e = (events *) ctx;
  if (event_type == CARRIER_LOST) {
    e->lost_count++;
    return;
  }
  e->count++;
  e->frequency = frequency;
}

static float uniform() {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return (float) (rng_state >> 8) / (float) (1 << 24) - 0.5f;
}

// sum of 4 uniforms, ~gaussian, variance 1/3 per component
static float noise() {
  return (uniform() + uniform() + uniform() + uniform());
}

typedef enum {
  SEGMENT_NOISE,
  SEGMENT_CARRIER,
  // random +-1 at 2000 baud: wideband, no tone
  SEGMENT_MODULATED
} segment_type;

// noise has power 2/3 per sample (variance 1/3 per component)
#define NOISE_POWER (2.0f / 3.0f)

static void append(segment_type type, float seconds, float amplitude, float frequency, float noise_gain) {
  size_t len = (size_t) (seconds * SAMPLE_RATE);
  signal_buffer = realloc(signal_buffer, sizeof(float complex) * (signal_len + len));
  TEST_ASSERT_NOT_NULL(signal_buffer);
  float symbol = 1.0f;
  for (size_t i = 0; i < len; i++) {
    float t = (float) (signal_len + i) / SAMPLE_RATE;
    float complex value = noise_gain * (noise() + I * noise());
    if (type == SEGMENT_CARRIER) {
      value += amplitude * cexpf(I * 2.0f * (float) M_PI * frequency * t);
    } else if (type == SEGMENT_MODULATED) {
      if (i % 10 == 0) {
        symbol = uniform() > 0 ? 1.0f : -1.0f;
      }
      value += amplitude * symbol * cexpf(I * 2.0f * (float) M_PI * frequency * t);
    }
    signal_buffer[signal_len + i] = value;
  }
  signal_len += len;
}

static void create_detector(events *e) {
  carrier_detector_settings settings = carrier_detector_default_settings(SAMPLE_RATE);
  TEST_ASSERT_EQUAL_INT(0, carrier_detector_create(&settings, &detector));
  carrier_detector_set_callback(on_carrier, e, detector);
}

static void process_in_chunks(size_t chunk) {
  for (size_t offset = 0; offset < signal_len; offset += chunk) {
    size_t len = signal_len - offset < chunk ? signal_len - offset : chunk;
    carrier_detector_process(signal_buffer + offset, len, detector);
  }
}

void test_default_settings() {
  carrier_detector_settings settings = carrier_detector_default_settings(20000);
  TEST_ASSERT_EQUAL_UINT32(256, settings.fft_size);
  settings = carrier_detector_default_settings(1920000);
  TEST_ASSERT_EQUAL_UINT32(4096, settings.fft_size);
  settings = carrier_detector_default_settings(1000);
  TEST_ASSERT_EQUAL_UINT32(64, settings.fft_size);
}

void test_invalid_settings() {
  carrier_detector_settings settings = carrier_detector_default_settings(SAMPLE_RATE);
  settings.fft_size = 8;
  TEST_ASSERT_NOT_EQUAL_INT(0, carrier_detector_create(&settings, &detector));
  settings = carrier_detector_default_settings(SAMPLE_RATE);
  settings.close_db = settings.open_db + 1;
  TEST_ASSERT_NOT_EQUAL_INT(0, carrier_detector_create(&settings, &detector));
  settings = carrier_detector_default_settings(SAMPLE_RATE);
  settings.sample_rate = 0;
  TEST_ASSERT_NOT_EQUAL_INT(0, carrier_detector_create(&settings, &detector));
  settings = carrier_detector_default_settings(SAMPLE_RATE);
  settings.confirm_frames = 0;
  TEST_ASSERT_NOT_EQUAL_INT(0, carrier_detector_create(&settings, &detector));
  TEST_ASSERT_NOT_EQUAL_INT(0, carrier_detector_create(NULL, &detector));
  TEST_ASSERT_NULL(detector);
}

void test_noise_only() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 3.0f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
  TEST_ASSERT_FLOAT_WITHIN(NOISE_POWER * 0.1f, NOISE_POWER, carrier_detector_get_noise_floor(detector));
}

void test_carrier_positive_frequency() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  // 10 dB above the noise
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 10.0f), 1234.5f, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(1, e.count);
  // bin is 78 Hz
  TEST_ASSERT_FLOAT_WITHIN(10.0f, 1234.5f, e.frequency);
  TEST_ASSERT_TRUE(carrier_detector_is_gate_open(detector));
}

void test_carrier_negative_frequency() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 10.0f), -3000.3f, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(1, e.count);
  TEST_ASSERT_FLOAT_WITHIN(10.0f, -3000.3f, e.frequency);
}

void test_carrier_at_center() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 10.0f), 0.0f, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(1, e.count);
  TEST_ASSERT_FLOAT_WITHIN(10.0f, 0.0f, e.frequency);
}

void test_chunking_does_not_matter() {
  events whole = {0};
  create_detector(&whole);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 10.0f), 2500.0f, 1.0f);
  process_in_chunks(signal_len);
  carrier_detector_destroy(detector);
  detector = NULL;

  events chunked = {0};
  create_detector(&chunked);
  process_in_chunks(97);
  TEST_ASSERT_EQUAL_INT(1, whole.count);
  TEST_ASSERT_EQUAL_INT(1, chunked.count);
  TEST_ASSERT_EQUAL_FLOAT(whole.frequency, chunked.frequency);
}

// the modulation which follows the carrier must not produce the second event,
// the next transmission after the pause must
void test_two_transmissions() {
  events e = {0};
  create_detector(&e);
  float amplitude = sqrtf(NOISE_POWER * 10.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.3f, amplitude, 1500.0f, 1.0f);
  append(SEGMENT_MODULATED, 0.5f, amplitude, 1500.0f, 1.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(1, e.count);
  TEST_ASSERT_EQUAL_INT(1, e.lost_count);
  TEST_ASSERT_FLOAT_WITHIN(10.0f, 1500.0f, e.frequency);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));

  free(signal_buffer);
  signal_buffer = NULL;
  signal_len = 0;
  append(SEGMENT_CARRIER, 0.3f, amplitude, -2000.0f, 1.0f);
  append(SEGMENT_MODULATED, 0.3f, amplitude, -2000.0f, 1.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(2, e.count);
  TEST_ASSERT_EQUAL_INT(2, e.lost_count);
  TEST_ASSERT_FLOAT_WITHIN(10.0f, -2000.0f, e.frequency);
}

// strong, but without the carrier
void test_modulation_without_carrier() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_MODULATED, 1.0f, sqrtf(NOISE_POWER * 10.0f), 1500.0f, 1.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
}

// the gate opens, but there is no narrow peak
void test_wideband_noise_burst() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_NOISE, 0.6f, 0, 0, 4.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
}

// below the open threshold (6 dB) the power gate stays closed even though the carrier is visible in the spectrum
void test_weak_carrier_does_not_open_gate() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 0.5f), 1000.0f, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
}

void test_short_spike_is_ignored() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  // shorter than open_frames * frame
  append(SEGMENT_CARRIER, 0.01f, sqrtf(NOISE_POWER * 100.0f), 1000.0f, 1.0f);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
}

// the floor drifts slowly, the gate must not open
void test_slow_noise_rise_is_tracked() {
  events e = {0};
  create_detector(&e);
  for (int i = 0; i < 20; i++) {
    append(SEGMENT_NOISE, 0.25f, 0, 0, 1.0f + 0.05f * (float) i);
  }
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(0, e.count);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
}

void test_reset() {
  events e = {0};
  create_detector(&e);
  append(SEGMENT_NOISE, 0.5f, 0, 0, 1.0f);
  append(SEGMENT_CARRIER, 0.4f, sqrtf(NOISE_POWER * 10.0f), 1000.0f, 1.0f);
  process_in_chunks(signal_len);
  TEST_ASSERT_EQUAL_INT(1, e.count);
  carrier_detector_reset(detector);
  TEST_ASSERT_FALSE(carrier_detector_is_gate_open(detector));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, carrier_detector_get_noise_floor(detector));
}

void test_null_safety() {
  carrier_detector_set_callback(NULL, NULL, NULL);
  carrier_detector_process(NULL, 0, NULL);
  carrier_detector_reset(NULL);
  TEST_ASSERT_EQUAL_INT(0, carrier_detector_is_gate_open(NULL));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, carrier_detector_get_noise_floor(NULL));
  carrier_detector_destroy(NULL);
}

void tearDown() {
  if (detector != NULL) {
    carrier_detector_destroy(detector);
    detector = NULL;
  }
  if (signal_buffer != NULL) {
    free(signal_buffer);
    signal_buffer = NULL;
  }
  signal_len = 0;
}

void setUp() {
  rng_state = 12345;
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_default_settings);
  RUN_TEST(test_invalid_settings);
  RUN_TEST(test_noise_only);
  RUN_TEST(test_carrier_positive_frequency);
  RUN_TEST(test_carrier_negative_frequency);
  RUN_TEST(test_carrier_at_center);
  RUN_TEST(test_chunking_does_not_matter);
  RUN_TEST(test_two_transmissions);
  RUN_TEST(test_modulation_without_carrier);
  RUN_TEST(test_wideband_noise_burst);
  RUN_TEST(test_weak_carrier_does_not_open_gate);
  RUN_TEST(test_short_spike_is_ignored);
  RUN_TEST(test_slow_noise_rise_is_tracked);
  RUN_TEST(test_reset);
  RUN_TEST(test_null_safety);
  return UNITY_END();
}
