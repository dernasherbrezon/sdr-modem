#include "gfsk_correlator.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <liquid/liquid.h>

// once the threshold is crossed, the peak is searched for this many symbols further. the training
// sequence repeats every 4 symbols, so its sidelobes can cross the threshold before the real peak
#define GFSK_CORRELATOR_PEAK_SEARCH_SYMBOLS 8
// smallest amplitude of the sync word (in units of the configured deviation) that is used for gain
// normalization. anything that correlates above the threshold is way above it
#define GFSK_CORRELATOR_MIN_AMPLITUDE 1e-3f

struct gfsk_correlator_t {
  unsigned int sps;
  float threshold;
  size_t max_input_buffer_length;

  size_t template_len;
  size_t peak_search_len;
  // the sampler runs this many samples behind the correlator: once the sync word is found, the burst
  // is sampled from the first symbol of the sync word with the estimated timing
  size_t sampler_lag;
  // samples kept between calls: the sampler lag, plus room for its interpolation
  size_t history_len;

  // input: history_len samples from the previous calls, then the samples of the current call. all
  // positions below are indexes into it
  float *samples;

  // sync word template (zero mean), as it looks at the input
  dotprod_rrrf correlator;
  float template_mean;
  float template_energy;

  // next window end to correlate
  size_t correlator_index;
  // no new detection before this index, so the same sync word is not detected twice
  size_t holdoff_index;
  bool peak_search;
  size_t peak_index;
  float peak_value;

  // per burst estimates from the sync word
  // position of the next symbol to output
  double sample_position;
  // frequency offset: dc of the discriminator output
  float dc;
  // 1 / amplitude of the sync word, so that the soft symbols are +-1 regardless of the deviation
  float gain;

  // soft symbols, +-1 for +-deviation
  int8_t *output;
  size_t output_len;
};

// the sync word as it comes out of the rx filter: every symbol is the tx frequency pulse
// (LIQUID_CPFSK_GMSK) convolved with the rx filter. symbol i is centered at i * sps + sps / 2
static int gfsk_correlator_create_template(float bt, unsigned int filter_delay, const float *rx_filter, unsigned int rx_filter_len, const uint8_t *sync_word, size_t sync_word_bits, struct gfsk_correlator_t *correlator) {
  unsigned int sps = correlator->sps;
  unsigned int tx_pulse_len = 2 * sps * filter_delay + 1;
  float *tx_pulse = malloc(sizeof(float) * tx_pulse_len);
  if (tx_pulse == NULL) {
    return -ENOMEM;
  }
  liquid_firdes_gmsktx(sps, filter_delay, bt, 0.0f, tx_pulse);
  float tx_pulse_sum = 0.0f;
  for (unsigned int i = 0; i < tx_pulse_len; i++) {
    tx_pulse_sum += tx_pulse[i];
  }
  // a run of the same symbol is +-1 at the discriminator output
  for (unsigned int i = 0; i < tx_pulse_len; i++) {
    tx_pulse[i] *= (float) sps / tx_pulse_sum;
  }

  unsigned int pulse_len = tx_pulse_len + rx_filter_len - 1;
  float *pulse = calloc(pulse_len, sizeof(float));
  if (pulse == NULL) {
    free(tx_pulse);
    return -ENOMEM;
  }
  for (unsigned int i = 0; i < tx_pulse_len; i++) {
    for (unsigned int j = 0; j < rx_filter_len; j++) {
      pulse[i + j] += tx_pulse[i] * rx_filter[j];
    }
  }
  free(tx_pulse);
  int pulse_center = (int) (pulse_len / 2);

  int template_len = (int) correlator->template_len;
  float *template = calloc(template_len, sizeof(float));
  if (template == NULL) {
    free(pulse);
    return -ENOMEM;
  }
  for (int i = 0; i < (int) sync_word_bits; i++) {
    float symbol = ((sync_word[i / 8] >> (7 - (i % 8))) & 1U) ? 1.0f : -1.0f;
    int center = i * (int) sps + (int) sps / 2;
    for (int j = 0; j < template_len; j++) {
      int pulse_index = j - center + pulse_center;
      if (pulse_index >= 0 && pulse_index < (int) pulse_len) {
        template[j] += symbol * pulse[pulse_index];
      }
    }
  }
  free(pulse);

  float mean = 0.0f;
  for (int i = 0; i < template_len; i++) {
    mean += template[i];
  }
  mean /= (float) template_len;
  float energy = 0.0f;
  for (int i = 0; i < template_len; i++) {
    template[i] -= mean;
    energy += template[i] * template[i];
  }
  correlator->template_mean = mean;
  correlator->template_energy = energy;
  correlator->correlator = dotprod_rrrf_create(template, template_len);
  free(template);
  if (correlator->correlator == NULL) {
    return -EINVAL;
  }
  return 0;
}

int gfsk_correlator_create(unsigned int sps, float bt, unsigned int filter_delay, const float *rx_filter, unsigned int rx_filter_len, const uint8_t *sync_word, size_t sync_word_bits, float threshold, size_t max_input_buffer_length, gfsk_correlator **correlator) {
  if (sps < 2 || sps % 2 != 0) {
    fprintf(stderr, "<3>gfsk correlator: samples per symbol must be even and at least 2: %u\n", sps);
    return -EINVAL;
  }
  if (sync_word == NULL || sync_word_bits == 0) {
    fprintf(stderr, "<3>gfsk correlator: sync word must not be empty\n");
    return -EINVAL;
  }
  struct gfsk_correlator_t *result = malloc(sizeof(struct gfsk_correlator_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct gfsk_correlator_t) {0};
  result->sps = sps;
  result->threshold = threshold;
  result->max_input_buffer_length = max_input_buffer_length;
  result->template_len = sync_word_bits * sps;
  result->peak_search_len = GFSK_CORRELATOR_PEAK_SEARCH_SYMBOLS * sps;
  result->sampler_lag = result->template_len + result->peak_search_len + sps;
  result->history_len = result->sampler_lag + 2 * sps;

  int code = gfsk_correlator_create_template(bt, filter_delay, rx_filter, rx_filter_len, sync_word, sync_word_bits, result);
  if (code != 0) {
    gfsk_correlator_destroy(result);
    return code;
  }

  // history starts with zeros: nothing correlates with them
  result->samples = calloc(result->history_len + max_input_buffer_length, sizeof(float));
  if (result->samples == NULL) {
    gfsk_correlator_destroy(result);
    return -ENOMEM;
  }
  result->correlator_index = result->history_len;
  result->holdoff_index = 0;
  result->sample_position = (double) result->history_len;
  result->dc = 0.0f;
  result->gain = 1.0f;

  // 1 symbol per sps samples, plus a few for the fractional sample position
  result->output_len = max_input_buffer_length / sps + 4;
  result->output = malloc(sizeof(int8_t) * result->output_len);
  if (result->output == NULL) {
    gfsk_correlator_destroy(result);
    return -ENOMEM;
  }

  *correlator = result;
  return 0;
}

// normalized correlation of the window that ends at end (inclusive) with the sync word template.
// the sign is the polarity. correlation is the raw dot product with the zero mean template
static float gfsk_correlator_correlate(gfsk_correlator *correlator, size_t end, float *correlation, float *mean) {
  const float *window = correlator->samples + end + 1 - correlator->template_len;
  float sum = 0.0f;
  float sum_squares = 0.0f;
  for (size_t i = 0; i < correlator->template_len; i++) {
    sum += window[i];
    sum_squares += window[i] * window[i];
  }
  float dot = 0.0f;
  dotprod_rrrf_execute(correlator->correlator, (float *) window, &dot);
  *correlation = dot;
  *mean = sum / (float) correlator->template_len;
  float variance = sum_squares - sum * sum / (float) correlator->template_len;
  if (variance <= 1e-9f) {
    return 0.0f;
  }
  return dot / sqrtf(variance * correlator->template_energy);
}

// the peak of the correlation is the sync word: estimate timing, frequency offset and amplitude of
// the burst from it
static void gfsk_correlator_sync(gfsk_correlator *correlator) {
  float correlation = 0.0f;
  float mean = 0.0f;
  float peak = fabsf(gfsk_correlator_correlate(correlator, correlator->peak_index, &correlation, &mean));
  float before_correlation = 0.0f;
  float after_correlation = 0.0f;
  float ignored = 0.0f;
  float before = fabsf(gfsk_correlator_correlate(correlator, correlator->peak_index - 1, &before_correlation, &ignored));
  float after = fabsf(gfsk_correlator_correlate(correlator, correlator->peak_index + 1, &after_correlation, &ignored));
  // parabolic interpolation of the fractional peak position
  float fraction = 0.0f;
  float denominator = before - 2.0f * peak + after;
  if (denominator < 0.0f) {
    fraction = 0.5f * (before - after) / denominator;
    if (fraction > 0.5f) {
      fraction = 0.5f;
    } else if (fraction < -0.5f) {
      fraction = -0.5f;
    }
  }

  // least squares fit of the window to amplitude * template + dc
  float amplitude = correlation / correlator->template_energy;
  correlator->dc = mean - amplitude * correlator->template_mean;
  if (fabsf(amplitude) > GFSK_CORRELATOR_MIN_AMPLITUDE) {
    correlator->gain = 1.0f / fabsf(amplitude);
  }

  // restart sampling from the first symbol of the sync word, so the sync word itself is in the output
  correlator->sample_position = (double) correlator->peak_index + (double) fraction - (double) (correlator->template_len - 1) + (double) (correlator->sps / 2);
  correlator->holdoff_index = correlator->peak_index + correlator->template_len;
  correlator->peak_search = false;
}

static void gfsk_correlator_output_symbol(gfsk_correlator *correlator, size_t *num_symbols) {
  size_t index = (size_t) correlator->sample_position;
  float mu = (float) (correlator->sample_position - (double) index);
  float value = correlator->samples[index] * (1.0f - mu) + correlator->samples[index + 1] * mu;
  float r = (value - correlator->dc) * correlator->gain * 127.0f;
  if (r > INT8_MAX) {
    r = INT8_MAX;
  } else if (r < INT8_MIN) {
    r = INT8_MIN;
  }
  correlator->output[*num_symbols] = (int8_t) rintf(r);
  (*num_symbols)++;
  correlator->sample_position += correlator->sps;
}

void gfsk_correlator_process(const float *input, size_t input_len, int8_t **output, size_t *output_len, gfsk_correlator *correlator) {
  if (input_len > correlator->max_input_buffer_length) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, correlator->max_input_buffer_length);
    *output = NULL;
    *output_len = 0;
    return;
  }
  memcpy(correlator->samples + correlator->history_len, input, sizeof(float) * input_len);

  size_t samples_len = correlator->history_len + input_len;
  size_t num_symbols = 0;
  for (; correlator->correlator_index < samples_len; correlator->correlator_index++) {
    size_t index = correlator->correlator_index;
    if (index >= correlator->holdoff_index) {
      float correlation = 0.0f;
      float mean = 0.0f;
      float value = fabsf(gfsk_correlator_correlate(correlator, index, &correlation, &mean));
      if (value >= correlator->threshold && (!correlator->peak_search || value > correlator->peak_value)) {
        correlator->peak_search = true;
        correlator->peak_index = index;
        correlator->peak_value = value;
      }
    }
    if (correlator->peak_search && index - correlator->peak_index >= correlator->peak_search_len) {
      gfsk_correlator_sync(correlator);
    }
    while (correlator->sample_position + (double) correlator->sampler_lag <= (double) index) {
      gfsk_correlator_output_symbol(correlator, &num_symbols);
    }
  }

  // keep the history for the next call
  size_t shift = samples_len - correlator->history_len;
  memmove(correlator->samples, correlator->samples + shift, sizeof(float) * correlator->history_len);
  correlator->correlator_index -= shift;
  correlator->sample_position -= (double) shift;
  correlator->holdoff_index = correlator->holdoff_index > shift ? correlator->holdoff_index - shift : 0;
  if (correlator->peak_search) {
    correlator->peak_index -= shift;
  }

  *output = correlator->output;
  *output_len = num_symbols;
}

void gfsk_correlator_destroy(gfsk_correlator *correlator) {
  if (correlator == NULL) {
    return;
  }
  if (correlator->correlator != NULL) {
    dotprod_rrrf_destroy(correlator->correlator);
  }
  if (correlator->samples != NULL) {
    free(correlator->samples);
  }
  if (correlator->output != NULL) {
    free(correlator->output);
  }
  free(correlator);
}
