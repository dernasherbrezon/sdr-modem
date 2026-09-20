// Bit error rate (BER) simulation for the modems: bpsk (BPSK, DPSK, SDPSK), oqpsk, psk_pm and gfsk.
//
// For every modem and every Eb/N0 point random bits are modulated, passed through an AWGN channel
// and demodulated. The measured BER is written into a .csv file next to the theoretical BER, so the
// implementation loss can be plotted / compared.
//
// usage: ber_modem [output.csv] [max_bits_per_point]
//
// This is a standalone executable, it is not registered in CTest because a full run takes a while.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <complex.h>
#include "../src/dsp/bpsk_modem.h"
#include "../src/dsp/oqpsk_modem.h"
#include "../src/dsp/psk_pm_modem.h"
#include "../src/dsp/gfsk_modem.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEFAULT_OUTPUT_FILE "ber_modem.csv"

// common rrc/loop settings of psk modems
#define RRC_BETA 0.35f
#define RRC_DELAY 5
#define COSTAS_BANDWIDTH 0.01f
#define SYMSYNC_FILTER_BANK_SIZE 32

#define PSK_SAMPLE_RATE 48000
#define PSK_BAUD_RATE 4800

// pcm/psk/pm
#define PSK_PM_SUBCARRIER_FREQUENCY 6000
#define PSK_PM_MODULATION_INDEX 1.0f
#define PSK_PM_CARRIER_PLL_BANDWIDTH 0.001f

// gfsk deviation is half of the baud rate: modulation index h = 1, i.e. orthogonal tones. sample
// rate is close to the signal bandwidth, as it is after the halfband decimation in the real pipeline
#define GFSK_SAMPLE_RATE 24000
#define GFSK_BAUD_RATE 4800
#define GFSK_DEVIATION 2400
#define GFSK_BANDWIDTH 9600
#define GFSK_BT 0.5f

#define EB_N0_STEP_DB 1.0

// bits are processed in chunks, like a real stream: modem state (AGC, timing and carrier loops)
// is carried over between chunks
#define CHUNK_BYTES 256
#define CHUNK_BITS (CHUNK_BYTES * 8)
// stop the point once enough errors were collected to get ~10% statistical accuracy
#define MIN_ERRORS 100
#define DEFAULT_MAX_BITS 4000000
// number of bits used to find the delay of the demodulated stream against the transmitted one
#define CALIBRATION_BITS 3000
// search range for the delay, in symbols. gfsk is the slowest: ~66 symbols of gaussian filters delay
#define MAX_LAG_SYMBOLS 128
// signal statistics are measured on this many chunks of noiseless signal
#define STATS_MEASUREMENT_CHUNKS 32

typedef void (*modulate_fn)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

typedef void (*demodulate_fn)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

typedef void (*destroy_fn)(void *modem);

// modulator and demodulator of the same configuration
typedef struct {
  void *mod;
  void *demod;
  modulate_fn modulate;
  demodulate_fn demodulate;
  destroy_fn destroy;
} modem_pair;

// measured on the noiseless modulated signal
typedef struct {
  // mean |x|^2
  double power;
  // mean of arg(x)^2. peak phase deviation squared, only meaningful for the phase modulated signal
  double phase_power;
} signal_stats;

typedef struct modem_case_t modem_case;

struct modem_case_t {
  const char *name;
  uint64_t sample_rate;
  uint32_t baud_rate;
  // 1 for bpsk/gfsk, 2 for oqpsk. bit_rate = baud_rate * bits_per_symbol
  unsigned int bits_per_symbol;
  // number of equivalent alignments of the demodulated stream against the transmitted one caused by
  // the carrier phase ambiguity: 1 (none), 2 (bpsk: inverted or not), 4 (oqpsk: rotated by 0/90/180/270)
  unsigned int alignment_variants;
  // let the AGC, symbol timing and carrier loops settle before counting errors
  size_t skip_bits;
  double eb_n0_min_db;
  double eb_n0_max_db;
  psk_modem_type psk_type;
  int (*create)(const modem_case *modem_case, uint32_t chunk_bytes, modem_pair *pair);
  // eb_n0 is linear (not dB)
  double (*theoretical_ber)(double eb_n0, const signal_stats *stats);
};

// xorshift64*: own generator so that results are the same on every platform
static uint64_t rng_next(uint64_t *state) {
  uint64_t x = *state;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *state = x;
  return x * 0x2545F4914F6CDD1DULL;
}

// uniform in (0, 1)
static double rng_uniform(uint64_t *state) {
  return ((double) (rng_next(state) >> 11) + 0.5) / 9007199254740992.0;
}

// complex gaussian noise, sigma_per_dim is the standard deviation of I and Q
static float complex noise_sample(uint64_t *state, double sigma_per_dim) {
  double radius = sigma_per_dim * sqrt(-2.0 * log(rng_uniform(state)));
  double angle = 2.0 * M_PI * rng_uniform(state);
  return (float) (radius * cos(angle)) + (float) (radius * sin(angle)) * I;
}

// Q(x): tail probability of the standard normal distribution
static double q_function(double x) {
  return 0.5 * erfc(x / sqrt(2.0));
}

// theory ////////////////////////////////////////////////////////////////////////////////////////

// BPSK and OQPSK (two independent BPSK on I and Q rails, Eb = Es / 2): coherent detection
static double theory_coherent_psk(double eb_n0, const signal_stats *stats) {
  (void) stats;
  return q_function(sqrt(2.0 * eb_n0));
}

// DPSK and SDPSK are detected differentially, using the previous noisy symbol as a phase
// reference. SDPSK is DPSK rotated by 90 degrees, so both have the same BER
static double theory_differential_psk(double eb_n0, const signal_stats *stats) {
  (void) stats;
  return 0.5 * exp(-eb_n0);
}

// PCM/PSK/PM: the phase discriminator turns the noise into a phase noise with the variance of the
// noise quadrature component, so the data BPSK subcarrier sees the effective Eb/N0 scaled by the
// mean square of the phase deviation (i.e. modulation index and subcarrier power loss). valid
// while the carrier is well above the noise, it ignores the threshold effect of the phase discriminator
static double theory_psk_pm(double eb_n0, const signal_stats *stats) {
  return q_function(sqrt(2.0 * eb_n0 * stats->phase_power));
}

// GFSK has no closed form. reference is a non-coherent detection of orthogonal binary FSK (h = 1),
// it doesn't take into account the gaussian pulse shaping (inter-symbol interference)
static double theory_gfsk(double eb_n0, const signal_stats *stats) {
  (void) stats;
  return 0.5 * exp(-eb_n0 / 2.0);
}

// modems ////////////////////////////////////////////////////////////////////////////////////////

static int create_bpsk(const modem_case *modem_case, uint32_t chunk_bytes, modem_pair *pair) {
  bpsk_modem_settings settings = {0};
  settings.sample_rate = modem_case->sample_rate;
  settings.baud_rate = modem_case->baud_rate;
  // no lowpass: the noise is shaped by the RRC matched filter only, which is what the theory assumes
  settings.bandwidth = 0;
  settings.rrc_beta = RRC_BETA;
  settings.rrc_delay = RRC_DELAY;
  settings.costas_bandwidth = COSTAS_BANDWIDTH;
  settings.symsync_filter_bank_size = SYMSYNC_FILTER_BANK_SIZE;
  settings.type = modem_case->psk_type;
  bpsk_modem *mod = NULL;
  bpsk_modem *demod = NULL;
  int code = bpsk_modem_create(&settings, chunk_bytes, &mod);
  if (code != 0) {
    return code;
  }
  // demodulator has to accept everything the modulator can produce
  code = bpsk_modem_create(&settings, (uint32_t) bpsk_modem_max_modulation_buffer_length(mod), &demod);
  if (code != 0) {
    bpsk_modem_destroy(mod);
    return code;
  }
  *pair = (modem_pair) {mod, demod, bpsk_modem_modulate, bpsk_modem_demodulate, bpsk_modem_destroy};
  return 0;
}

static int create_oqpsk(const modem_case *modem_case, uint32_t chunk_bytes, modem_pair *pair) {
  oqpsk_modem_settings settings = {0};
  settings.sample_rate = modem_case->sample_rate;
  settings.baud_rate = modem_case->baud_rate;
  settings.rrc_beta = RRC_BETA;
  settings.rrc_delay = RRC_DELAY;
  settings.costas_bandwidth = COSTAS_BANDWIDTH;
  oqpsk_modem *mod = NULL;
  oqpsk_modem *demod = NULL;
  int code = oqpsk_modem_create(&settings, chunk_bytes, &mod);
  if (code != 0) {
    return code;
  }
  code = oqpsk_modem_create(&settings, (uint32_t) oqpsk_modem_max_modulation_buffer_length(mod), &demod);
  if (code != 0) {
    oqpsk_modem_destroy(mod);
    return code;
  }
  *pair = (modem_pair) {mod, demod, oqpsk_modem_modulate, oqpsk_modem_demodulate, oqpsk_modem_destroy};
  return 0;
}

static int create_psk_pm(const modem_case *modem_case, uint32_t chunk_bytes, modem_pair *pair) {
  psk_pm_modem_settings settings = {0};
  settings.sample_rate = modem_case->sample_rate;
  settings.baud_rate = modem_case->baud_rate;
  settings.subcarrier_frequency = PSK_PM_SUBCARRIER_FREQUENCY;
  settings.subcarrier_bandwidth = 0;
  settings.modulation_index = PSK_PM_MODULATION_INDEX;
  settings.carrier_pll_bandwidth = PSK_PM_CARRIER_PLL_BANDWIDTH;
  settings.rrc_beta = RRC_BETA;
  settings.rrc_delay = RRC_DELAY;
  settings.costas_bandwidth = COSTAS_BANDWIDTH;
  settings.symsync_filter_bank_size = SYMSYNC_FILTER_BANK_SIZE;
  psk_pm_modem *mod = NULL;
  psk_pm_modem *demod = NULL;
  int code = psk_pm_modem_create(&settings, chunk_bytes, &mod);
  if (code != 0) {
    return code;
  }
  code = psk_pm_modem_create(&settings, (uint32_t) psk_pm_modem_max_modulation_buffer_length(mod), &demod);
  if (code != 0) {
    psk_pm_modem_destroy(mod);
    return code;
  }
  *pair = (modem_pair) {mod, demod, psk_pm_modem_modulate, psk_pm_modem_demodulate, psk_pm_modem_destroy};
  return 0;
}

static int create_gfsk(const modem_case *modem_case, uint32_t chunk_bytes, modem_pair *pair) {
  GfskModemSettings settings = {
      .sample_rate = modem_case->sample_rate,
      .baud_rate = modem_case->baud_rate,
      .deviation = GFSK_DEVIATION,
      .bandwidth = GFSK_BANDWIDTH,
      .use_dc_block = true,
      .bt = GFSK_BT
  };
  gfsk_modem *mod = NULL;
  gfsk_modem *demod = NULL;
  int code = gfsk_modem_create(&settings, settings.sample_rate, chunk_bytes, &mod);
  if (code != 0) {
    return code;
  }
  code = gfsk_modem_create(&settings, settings.sample_rate, (uint32_t) gfsk_modem_max_modulation_buffer_length(mod), &demod);
  if (code != 0) {
    gfsk_modem_destroy(mod);
    return code;
  }
  *pair = (modem_pair) {mod, demod, gfsk_modem_modulate, gfsk_modem_demodulate, gfsk_modem_destroy};
  return 0;
}

static const modem_case CASES[] = {
    {"bpsk",   PSK_SAMPLE_RATE, PSK_BAUD_RATE, 1, 2, 300, 0.0,  10.0, BPSK,  create_bpsk,   theory_coherent_psk},
    {"dpsk",   PSK_SAMPLE_RATE, PSK_BAUD_RATE, 1, 1, 300, 0.0,  10.0, DPSK,  create_bpsk,   theory_differential_psk},
    {"sdpsk",  PSK_SAMPLE_RATE, PSK_BAUD_RATE, 1, 1, 300, 0.0,  10.0, SDPSK, create_bpsk,   theory_differential_psk},
    {"oqpsk",  PSK_SAMPLE_RATE, PSK_BAUD_RATE, 2, 4, 300, 0.0,  10.0, BPSK,  create_oqpsk,  theory_coherent_psk},
    {"psk_pm", PSK_SAMPLE_RATE, PSK_BAUD_RATE, 1, 2, 300, 10.0, 22.0, BPSK,  create_psk_pm, theory_psk_pm},
    {"gfsk",   GFSK_SAMPLE_RATE, GFSK_BAUD_RATE, 1, 1, 300, 0.0, 14.0, BPSK,  create_gfsk,   theory_gfsk},
};

// simulation ////////////////////////////////////////////////////////////////////////////////////

static void fill_random_chunk(uint64_t *rng, uint8_t *bytes, uint8_t *bits) {
  for (size_t i = 0; i < CHUNK_BYTES; i++) {
    bytes[i] = (uint8_t) (rng_next(rng) >> 32);
    for (int bit = 0; bit < 8; bit++) {
      bits[i * 8 + bit] = (bytes[i] >> (7 - bit)) & 1U;
    }
  }
}

static int measure_signal_stats(const modem_case *modem_case, signal_stats *stats) {
  modem_pair pair;
  int code = modem_case->create(modem_case, CHUNK_BYTES, &pair);
  if (code != 0) {
    return code;
  }
  uint64_t rng = 0x9E3779B97F4A7C15ULL;
  uint8_t bytes[CHUNK_BYTES];
  uint8_t bits[CHUNK_BITS];
  double power_sum = 0.0;
  double phase_power_sum = 0.0;
  size_t total = 0;
  for (int chunk = 0; chunk < STATS_MEASUREMENT_CHUNKS; chunk++) {
    fill_random_chunk(&rng, bytes, bits);
    float complex *output = NULL;
    size_t output_len = 0;
    pair.modulate(bytes, CHUNK_BYTES, &output, &output_len, pair.mod);
    for (size_t i = 0; i < output_len; i++) {
      double re = crealf(output[i]);
      double im = cimagf(output[i]);
      double phase = atan2(im, re);
      power_sum += re * re + im * im;
      phase_power_sum += phase * phase;
    }
    total += output_len;
  }
  pair.destroy(pair.mod);
  pair.destroy(pair.demod);
  if (total == 0 || power_sum <= 0.0) {
    return -1;
  }
  stats->power = power_sum / (double) total;
  stats->phase_power = phase_power_sum / (double) total;
  return 0;
}

// The bit that is expected at the position i of the demodulated stream, if it is delayed by lag
// symbols and rotated by variant. Returns false if there is no such transmitted bit (yet).
//
// 1 bit per symbol: variant 1 is the inverted stream (180 degrees carrier phase ambiguity).
// 2 bits per symbol, (I, Q): variant is the rotation of the constellation by variant * 90 degrees.
static bool expected_bit(const uint8_t *tx_bits, size_t tx_len, unsigned int bits_per_symbol, size_t i, size_t lag, unsigned int variant, unsigned int *bit) {
  size_t symbol = i / bits_per_symbol;
  if (symbol < lag) {
    return false;
  }
  size_t base = (symbol - lag) * bits_per_symbol;
  if (base + bits_per_symbol > tx_len) {
    return false;
  }
  if (bits_per_symbol == 1) {
    *bit = tx_bits[base] ^ variant;
    return true;
  }
  unsigned int in_i = tx_bits[base];
  unsigned int in_q = tx_bits[base + 1];
  unsigned int out_i;
  unsigned int out_q;
  switch (variant) {
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
  *bit = (i % 2 == 0) ? out_i : out_q;
  return true;
}

// Demodulated stream is delayed against the transmitted one by an unknown number of symbols and
// can be inverted/rotated by the carrier recovery. Find the alignment with the fewest mismatches on
// the first part of the stream.
static void find_alignment(const modem_case *modem_case, const uint8_t *tx_bits, size_t tx_len, const uint8_t *rx_bits, size_t rx_len, size_t *best_lag, unsigned int *best_variant) {
  size_t best_mismatches = (size_t) -1;
  *best_lag = 0;
  *best_variant = 0;
  for (size_t lag = 0; lag <= MAX_LAG_SYMBOLS; lag++) {
    for (unsigned int variant = 0; variant < modem_case->alignment_variants; variant++) {
      size_t mismatches = 0;
      size_t total = 0;
      for (size_t i = modem_case->skip_bits; i < rx_len; i++) {
        unsigned int expected;
        if (!expected_bit(tx_bits, tx_len, modem_case->bits_per_symbol, i, lag, variant, &expected)) {
          break;
        }
        mismatches += rx_bits[i] != expected;
        total++;
      }
      if (total > 0 && mismatches < best_mismatches) {
        best_mismatches = mismatches;
        *best_lag = lag;
        *best_variant = variant;
      }
    }
  }
}

static int simulate_point(const modem_case *modem_case, const signal_stats *stats, double eb_n0_db, size_t max_bits, uint64_t seed, size_t *total_bits, size_t *errors) {
  // sum(|x|^2) over one bit period is power * (sample_rate / bit_rate), and noise power per complex
  // sample is N0 in the same units, so Eb/N0 = power * (sample_rate / bit_rate) / noise_power
  double eb_n0 = pow(10.0, eb_n0_db / 10.0);
  double bit_rate = (double) modem_case->baud_rate * modem_case->bits_per_symbol;
  double noise_power = stats->power * ((double) modem_case->sample_rate / bit_rate) / eb_n0;
  double sigma_per_dim = sqrt(noise_power / 2.0);

  modem_pair pair;
  int code = modem_case->create(modem_case, CHUNK_BYTES, &pair);
  if (code != 0) {
    return code;
  }

  // rx stream can be a few bits longer than the tx one
  size_t capacity = max_bits + CHUNK_BITS + MAX_LAG_SYMBOLS * modem_case->bits_per_symbol;
  uint8_t *tx_bits = malloc(capacity);
  uint8_t *rx_bits = malloc(capacity);
  if (tx_bits == NULL || rx_bits == NULL) {
    free(tx_bits);
    free(rx_bits);
    pair.destroy(pair.mod);
    pair.destroy(pair.demod);
    return -1;
  }
  size_t tx_len = 0;
  size_t rx_len = 0;

  uint64_t data_rng = seed;
  uint64_t noise_rng = seed ^ 0xD1B54A32D192ED03ULL;
  uint8_t bytes[CHUNK_BYTES];

  bool calibrated = false;
  size_t lag = 0;
  unsigned int variant = 0;
  size_t counted = 0;
  size_t error_count = 0;

  while (tx_len < max_bits && error_count < MIN_ERRORS) {
    fill_random_chunk(&data_rng, bytes, tx_bits + tx_len);
    tx_len += CHUNK_BITS;

    float complex *signal = NULL;
    size_t signal_len = 0;
    pair.modulate(bytes, CHUNK_BYTES, &signal, &signal_len, pair.mod);
    for (size_t i = 0; i < signal_len; i++) {
      signal[i] += noise_sample(&noise_rng, sigma_per_dim);
    }

    int8_t *soft_bits = NULL;
    size_t soft_bits_len = 0;
    pair.demodulate(signal, signal_len, &soft_bits, &soft_bits_len, pair.demod);
    for (size_t i = 0; i < soft_bits_len && rx_len < capacity; i++) {
      rx_bits[rx_len++] = soft_bits[i] >= 0 ? 1U : 0U;
    }

    if (!calibrated && rx_len >= modem_case->skip_bits + CALIBRATION_BITS) {
      find_alignment(modem_case, tx_bits, tx_len, rx_bits, rx_len, &lag, &variant);
      calibrated = true;
      counted = modem_case->skip_bits;
    }
    if (calibrated) {
      for (; counted < rx_len; counted++) {
        unsigned int expected;
        if (!expected_bit(tx_bits, tx_len, modem_case->bits_per_symbol, counted, lag, variant, &expected)) {
          break;
        }
        error_count += rx_bits[counted] != expected;
      }
    }
  }

  *total_bits = calibrated ? counted - modem_case->skip_bits : 0;
  *errors = error_count;

  free(tx_bits);
  free(rx_bits);
  pair.destroy(pair.mod);
  pair.destroy(pair.demod);
  return calibrated && *total_bits > 0 ? 0 : -1;
}

int main(int argc, char *argv[]) {
  const char *output_file = argc > 1 ? argv[1] : DEFAULT_OUTPUT_FILE;
  size_t max_bits = DEFAULT_MAX_BITS;
  if (argc > 2) {
    long parsed = atol(argv[2]);
    if (parsed <= 1000 + CALIBRATION_BITS) {
      fprintf(stderr, "max_bits_per_point must be more than %d\n", 1000 + CALIBRATION_BITS);
      return EXIT_FAILURE;
    }
    max_bits = (size_t) parsed;
  }

  FILE *file = fopen(output_file, "w");
  if (file == NULL) {
    fprintf(stderr, "unable to open output file: %s\n", output_file);
    return EXIT_FAILURE;
  }
  fprintf(file, "modem,eb_n0_db,bits,errors,ber_practical,ber_theoretical\n");

  int result = EXIT_SUCCESS;
  for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
    const modem_case *modem_case = &CASES[c];
    signal_stats stats;
    if (measure_signal_stats(modem_case, &stats) != 0) {
      fprintf(stderr, "%s: unable to create modem or measure the signal\n", modem_case->name);
      result = EXIT_FAILURE;
      continue;
    }
    for (double eb_n0_db = modem_case->eb_n0_min_db; eb_n0_db <= modem_case->eb_n0_max_db + 1e-9; eb_n0_db += EB_N0_STEP_DB) {
      size_t bits = 0;
      size_t errors = 0;
      if (simulate_point(modem_case, &stats, eb_n0_db, max_bits, 0x853C49E6748FEA9BULL + c, &bits, &errors) != 0) {
        fprintf(stderr, "%s: simulation failed at Eb/N0 = %.1f dB\n", modem_case->name, eb_n0_db);
        result = EXIT_FAILURE;
        continue;
      }
      double practical = (double) errors / (double) bits;
      double theoretical = modem_case->theoretical_ber(pow(10.0, eb_n0_db / 10.0), &stats);
      fprintf(file, "%s,%.1f,%zu,%zu,%.6e,%.6e\n", modem_case->name, eb_n0_db, bits, errors, practical, theoretical);
      fflush(file);
      printf("%-6s Eb/N0 %4.1f dB: bits %8zu errors %6zu BER %.3e (theory %.3e)\n", modem_case->name, eb_n0_db, bits, errors, practical, theoretical);
    }
  }

  fclose(file);
  printf("results written into %s\n", output_file);
  return result;
}
