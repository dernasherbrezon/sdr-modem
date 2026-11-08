#include "carrier_detector.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <liquid/liquid.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CARRIER_DETECTOR_MIN_FFT_SIZE 16
#define CARRIER_DETECTOR_DEFAULT_MIN_FFT_SIZE 64
#define CARRIER_DETECTOR_DEFAULT_MAX_FFT_SIZE 4096
#define CARRIER_DETECTOR_TARGET_BIN_WIDTH_HZ 100.0f
#define CARRIER_DETECTOR_DEFAULT_CHECK_SECONDS 0.5f
// per frame. the floor follows a drop quickly and a rise slowly, so that a weak signal does not drag the floor up
#define NOISE_FLOOR_FALL 0.25f
#define NOISE_FLOOR_RISE 0.02f
#define NOISE_FLOOR_MIN 1e-20f
// the Hann main lobe is 4 bins wide: the peak +- 2 bins holds the whole tone wherever it falls between the bins
#define CARRIER_REGION_BINS 2
// median / mean of the exponential distribution: ln(2)
#define MEDIAN_TO_MEAN_EXPONENTIAL 0.693147f

typedef enum {
  STATE_IDLE = 0,
  // the gate is open, waiting for the carrier
  STATE_CHECKING,
  // the gate is open and the carrier was either reported or not found. waiting for the gate to close
  STATE_HOLD
} detector_state;

struct carrier_detector_t {
  carrier_detector_settings settings;
  float open_ratio;
  float close_ratio;

  fftplan plan;
  // fft input: the frame is collected here and the window is applied in place
  float complex *frame;
  float complex *spectrum;
  float *window;
  float *power;
  float *sorted;
  uint32_t frame_fill;

  detector_state state;
  bool floor_valid;
  float noise_floor;
  uint32_t open_count;
  uint32_t low_count;
  uint32_t checked;
  uint32_t confirmed;
  float frequency_sum;
  uint32_t last_peak_bin;
  // CARRIER_DETECTED was fired for the current transmission, so CARRIER_LOST is due when the gate closes
  bool carrier_reported;

  void (*callback)(carrier_detector_event_type event_type, float frequency_offset, void *ctx);
  void *callback_ctx;
};

carrier_detector_settings carrier_detector_default_settings(uint64_t sample_rate) {
  carrier_detector_settings result = {0};
  result.sample_rate = sample_rate;
  uint32_t fft_size = CARRIER_DETECTOR_DEFAULT_MIN_FFT_SIZE;
  while (fft_size < CARRIER_DETECTOR_DEFAULT_MAX_FFT_SIZE && (float) sample_rate / (float) fft_size > CARRIER_DETECTOR_TARGET_BIN_WIDTH_HZ) {
    fft_size *= 2;
  }
  result.fft_size = fft_size;
  result.open_db = 6.0f;
  result.close_db = 3.0f;
  result.min_peak_db = 15.0f;
  result.min_carrier_fraction = 0.5f;
  result.open_frames = 2;
  result.close_frames = 4;
  result.confirm_frames = 3;
  result.max_drift_bins = 2;
  uint32_t check_frames = (uint32_t) ceilf(CARRIER_DETECTOR_DEFAULT_CHECK_SECONDS * (float) sample_rate / (float) fft_size);
  if (check_frames < result.confirm_frames + 1) {
    check_frames = result.confirm_frames + 1;
  }
  result.max_check_frames = check_frames;
  return result;
}

static float db_to_ratio(float db) {
  return powf(10.0f, db / 10.0f);
}

static int compare_floats(const void *a, const void *b) {
  float x = *(const float *) a;
  float y = *(const float *) b;
  return (x > y) - (x < y);
}

int carrier_detector_create(const carrier_detector_settings *settings, carrier_detector **detector) {
  if (settings == NULL || detector == NULL) {
    return -EINVAL;
  }
  if (settings->sample_rate == 0 || settings->fft_size < CARRIER_DETECTOR_MIN_FFT_SIZE || settings->close_db > settings->open_db
      || settings->min_carrier_fraction <= 0.0f || settings->min_carrier_fraction > 1.0f || settings->open_frames == 0 || settings->close_frames == 0 || settings->confirm_frames == 0
      || settings->max_check_frames < settings->confirm_frames) {
    return -EINVAL;
  }
  struct carrier_detector_t *result = malloc(sizeof(struct carrier_detector_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct carrier_detector_t){0};
  result->settings = *settings;
  result->open_ratio = db_to_ratio(settings->open_db);
  result->close_ratio = db_to_ratio(settings->close_db);

  uint32_t n = settings->fft_size;
  result->frame = malloc(sizeof(float complex) * n);
  result->spectrum = malloc(sizeof(float complex) * n);
  result->window = malloc(sizeof(float) * n);
  result->power = malloc(sizeof(float) * n);
  result->sorted = malloc(sizeof(float) * n);
  if (result->frame == NULL || result->spectrum == NULL || result->window == NULL || result->power == NULL || result->sorted == NULL) {
    carrier_detector_destroy(result);
    return -ENOMEM;
  }
  // periodic Hann: a pure tone leaks into the neighbour bins only, which is what the interpolation expects
  for (uint32_t i = 0; i < n; i++) {
    result->window[i] = 0.5f - 0.5f * cosf(2.0f * (float) M_PI * (float) i / (float) n);
  }
  result->plan = fft_create_plan(n, result->frame, result->spectrum, LIQUID_FFT_FORWARD, 0);
  if (result->plan == NULL) {
    carrier_detector_destroy(result);
    return -ENOMEM;
  }
  *detector = result;
  return 0;
}

void carrier_detector_set_callback(void (*callback)(carrier_detector_event_type event_type, float frequency_offset, void *ctx), void *ctx, carrier_detector *detector) {
  if (detector == NULL) {
    return;
  }
  detector->callback = callback;
  detector->callback_ctx = ctx;
}

static void reset_state(carrier_detector *detector) {
  detector->state = STATE_IDLE;
  detector->open_count = 0;
  detector->low_count = 0;
  detector->checked = 0;
  detector->confirmed = 0;
  detector->frequency_sum = 0.0f;
  detector->carrier_reported = false;
}

void carrier_detector_reset(carrier_detector *detector) {
  if (detector == NULL) {
    return;
  }
  reset_state(detector);
  detector->floor_valid = false;
  detector->noise_floor = 0.0f;
  detector->frame_fill = 0;
}

static uint32_t circular_distance(uint32_t a, uint32_t b, uint32_t n) {
  uint32_t d = a > b ? a - b : b - a;
  return d < n - d ? d : n - d;
}

// finds the strongest bin of the current frame and checks that it stands out of the noise.
// returns true and the frequency of the peak (Hz) if it does
static bool analyze_spectrum(carrier_detector *detector, uint32_t *peak_bin, float *frequency) {
  uint32_t n = detector->settings.fft_size;
  for (uint32_t i = 0; i < n; i++) {
    detector->frame[i] *= detector->window[i];
  }
  fft_execute(detector->plan);

  uint32_t peak = 0;
  for (uint32_t i = 0; i < n; i++) {
    float re = crealf(detector->spectrum[i]);
    float im = cimagf(detector->spectrum[i]);
    float p = re * re + im * im;
    detector->power[i] = p;
    detector->sorted[i] = p;
    if (p > detector->power[peak]) {
      peak = i;
    }
  }
  qsort(detector->sorted, n, sizeof(float), compare_floats);
  float median = detector->sorted[n / 2];
  if (median < NOISE_FLOOR_MIN) {
    median = NOISE_FLOOR_MIN;
  }
  if (detector->power[peak] < median * db_to_ratio(detector->settings.min_peak_db)) {
    return false;
  }

  // a peak above the noise is not enough: a narrow modulated signal has it too. a carrier holds most of the signal power
  // in a few bins. the noise is estimated by the median (bin power of the noise is exponentially distributed)
  float noise_per_bin = median / MEDIAN_TO_MEAN_EXPONENTIAL;
  float total = 0.0f;
  for (uint32_t i = 0; i < n; i++) {
    total += detector->power[i];
  }
  float region = 0.0f;
  for (int32_t i = -CARRIER_REGION_BINS; i <= CARRIER_REGION_BINS; i++) {
    region += detector->power[(uint32_t) (((int64_t) peak + i + (int64_t) n) % (int64_t) n)];
  }
  float signal_total = total - noise_per_bin * (float) n;
  float signal_region = region - noise_per_bin * (float) (2 * CARRIER_REGION_BINS + 1);
  if (signal_total <= 0.0f || signal_region < detector->settings.min_carrier_fraction * signal_total) {
    return false;
  }

  // parabolic interpolation of the log power of the peak and its neighbours
  float delta = 0.0f;
  float left = logf(detector->power[(peak + n - 1) % n] + NOISE_FLOOR_MIN);
  float center = logf(detector->power[peak] + NOISE_FLOOR_MIN);
  float right = logf(detector->power[(peak + 1) % n] + NOISE_FLOOR_MIN);
  float denominator = left - 2.0f * center + right;
  if (denominator < -1e-6f) {
    delta = 0.5f * (left - right) / denominator;
    if (delta > 0.5f) {
      delta = 0.5f;
    } else if (delta < -0.5f) {
      delta = -0.5f;
    }
  }
  float signed_bin = (float) (peak >= n / 2 ? (int64_t) peak - (int64_t) n : (int64_t) peak) + delta;
  *peak_bin = peak;
  *frequency = signed_bin * (float) detector->settings.sample_rate / (float) n;
  return true;
}

static void update_noise_floor(carrier_detector *detector, float power) {
  if (!detector->floor_valid) {
    detector->noise_floor = power;
    detector->floor_valid = true;
  } else {
    float rate = power < detector->noise_floor ? NOISE_FLOOR_FALL : NOISE_FLOOR_RISE;
    detector->noise_floor += (power - detector->noise_floor) * rate;
  }
  if (detector->noise_floor < NOISE_FLOOR_MIN) {
    detector->noise_floor = NOISE_FLOOR_MIN;
  }
}

// returns true if the gate closed long enough
static bool update_close_counter(carrier_detector *detector, float power) {
  if (power < detector->noise_floor * detector->close_ratio) {
    detector->low_count++;
  } else {
    detector->low_count = 0;
  }
  return detector->low_count >= detector->settings.close_frames;
}

static void process_frame(carrier_detector *detector) {
  uint32_t n = detector->settings.fft_size;
  float power = 0.0f;
  for (uint32_t i = 0; i < n; i++) {
    float re = crealf(detector->frame[i]);
    float im = cimagf(detector->frame[i]);
    power += re * re + im * im;
  }
  power /= (float) n;

  switch (detector->state) {
    case STATE_IDLE: {
      if (!detector->floor_valid || power <= detector->noise_floor * detector->open_ratio) {
        detector->open_count = 0;
        update_noise_floor(detector, power);
        break;
      }
      detector->open_count++;
      if (detector->open_count >= detector->settings.open_frames) {
        reset_state(detector);
        detector->state = STATE_CHECKING;
      }
      break;
    }
    case STATE_CHECKING: {
      if (update_close_counter(detector, power)) {
        reset_state(detector);
        break;
      }
      if (detector->low_count > 0) {
        // fading out. not a carrier frame
        detector->confirmed = 0;
        detector->frequency_sum = 0.0f;
        break;
      }
      detector->checked++;
      uint32_t peak_bin = 0;
      float frequency = 0.0f;
      if (analyze_spectrum(detector, &peak_bin, &frequency)) {
        if (detector->confirmed > 0 && circular_distance(peak_bin, detector->last_peak_bin, n) > detector->settings.max_drift_bins) {
          // another peak. start over from this frame
          detector->confirmed = 0;
          detector->frequency_sum = 0.0f;
        }
        detector->confirmed++;
        detector->frequency_sum += frequency;
        detector->last_peak_bin = peak_bin;
        if (detector->confirmed >= detector->settings.confirm_frames) {
          detector->state = STATE_HOLD;
          detector->low_count = 0;
          detector->carrier_reported = true;
          if (detector->callback != NULL) {
            detector->callback(CARRIER_DETECTED, detector->frequency_sum / (float) detector->confirmed, detector->callback_ctx);
          }
          break;
        }
      } else {
        detector->confirmed = 0;
        detector->frequency_sum = 0.0f;
      }
      if (detector->checked >= detector->settings.max_check_frames) {
        // no carrier in this transmission. ignore it
        detector->state = STATE_HOLD;
        detector->low_count = 0;
      }
      break;
    }
    case STATE_HOLD: {
      if (update_close_counter(detector, power)) {
        bool lost = detector->carrier_reported;
        reset_state(detector);
        // fired after the state is reset, so the callback sees a consistent detector. no offset for the lost carrier
        if (lost && detector->callback != NULL) {
          detector->callback(CARRIER_LOST, 0.0f, detector->callback_ctx);
        }
      }
      break;
    }
  }
}

void carrier_detector_process(const float complex *input, size_t input_len, carrier_detector *detector) {
  if (detector == NULL || input == NULL) {
    return;
  }
  uint32_t n = detector->settings.fft_size;
  size_t offset = 0;
  while (offset < input_len) {
    size_t chunk = n - detector->frame_fill;
    if (chunk > input_len - offset) {
      chunk = input_len - offset;
    }
    memcpy(detector->frame + detector->frame_fill, input + offset, sizeof(float complex) * chunk);
    detector->frame_fill += (uint32_t) chunk;
    offset += chunk;
    if (detector->frame_fill == n) {
      process_frame(detector);
      detector->frame_fill = 0;
    }
  }
}

int carrier_detector_is_gate_open(carrier_detector *detector) {
  if (detector == NULL) {
    return 0;
  }
  return detector->state != STATE_IDLE;
}

float carrier_detector_get_noise_floor(carrier_detector *detector) {
  if (detector == NULL || !detector->floor_valid) {
    return 0.0f;
  }
  return detector->noise_floor;
}

void carrier_detector_destroy(carrier_detector *detector) {
  if (detector == NULL) {
    return;
  }
  if (detector->plan != NULL) {
    fft_destroy_plan(detector->plan);
  }
  free(detector->frame);
  free(detector->spectrum);
  free(detector->window);
  free(detector->power);
  free(detector->sorted);
  free(detector);
}
