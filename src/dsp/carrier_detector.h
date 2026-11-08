#ifndef DSP_CARRIER_DETECTOR_H_
#define DSP_CARRIER_DETECTOR_H_

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

// Modulation independent carrier detector. Every transmission is expected to start with an unmodulated carrier.
//
// 1. power gate: mean power of every frame against an adaptive noise floor (RSSI-like). The floor is tracked only
//    while the gate is closed. The gate opens after open_frames consecutive frames above floor + open_db and
//    closes after close_frames consecutive frames below floor + close_db (hysteresis)
// 2. carrier check: while the gate is open, the spectrum of every frame is checked for a single narrow peak:
//    peak / median bin power >= min_peak_db and the peak holds at least min_carrier_fraction of the signal power.
//    confirm_frames consecutive frames with a peak at (nearly) the same bin confirm the carrier. The callback is fired once with the coarse frequency offset
// 3. the detector stays quiet until the gate closes, so the modulation which follows the carrier
//    cannot trigger a second CARRIER_DETECTED. When the gate closes, CARRIER_LOST is fired (frequency_offset is 0)
// If the carrier is not confirmed within max_check_frames the transmission is ignored (no event, including no CARRIER_LOST).
// carrier_detector_reset drops the state silently: no CARRIER_LOST is fired.
//
// the input is expected to be channel-limited (e.g. after half-band decimation): the gate measures all power in the band
typedef struct carrier_detector_t carrier_detector;

typedef enum {
  CARRIER_DETECTED = 0,
  CARRIER_LOST = 1
} carrier_detector_event_type;

typedef struct {
  uint64_t sample_rate;
  // frame length in samples, at least 16, power of 2 is the fastest. frequency resolution is sample_rate / fft_size
  uint32_t fft_size;
  // power gate opens when frame power > noise floor + open_db
  float open_db;
  // power gate closes when frame power < noise floor + close_db. must be <= open_db
  float close_db;
  // peak bin power / median bin power, dB. noise alone gives ~10 dB for fft_size 256 and ~11 dB for 4096
  float min_peak_db;
  // (0, 1]. the peak +- 2 bins must hold at least this part of the signal power (frame power without the noise).
  // tells the carrier from a narrow modulated signal, which has a peak too
  float min_carrier_fraction;
  uint32_t open_frames;
  uint32_t close_frames;
  uint32_t confirm_frames;
  // max frames spent checking for the carrier after the gate opened
  uint32_t max_check_frames;
  // consecutive frames can differ by at most this many bins
  uint32_t max_drift_bins;
} carrier_detector_settings;

// defaults: ~100 Hz bin width (fft_size is clamped to [64, 4096]), open 6 dB, close 3 dB, peak 15 dB
carrier_detector_settings carrier_detector_default_settings(uint64_t sample_rate);

int carrier_detector_create(const carrier_detector_settings *settings, carrier_detector **detector);

// the callback runs synchronously inside carrier_detector_process, from the calling thread
void carrier_detector_set_callback(void (*callback)(carrier_detector_event_type event_type, float frequency_offset, void *ctx), void *ctx, carrier_detector *detector);

// streaming: any input length, partial frames are kept for the next call
void carrier_detector_process(const float complex *input, size_t input_len, carrier_detector *detector);

// forgets the noise floor and the state. use after the retune
void carrier_detector_reset(carrier_detector *detector);

// 1 while the power gate is open, i.e. the signal is above the noise floor
int carrier_detector_is_gate_open(carrier_detector *detector);

// noise floor, mean power per sample (linear). 0 before the first frame
float carrier_detector_get_noise_floor(carrier_detector *detector);

void carrier_detector_destroy(carrier_detector *detector);

#endif /* DSP_CARRIER_DETECTOR_H_ */
