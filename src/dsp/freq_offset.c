#include "freq_offset.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <liquid/liquid.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct freq_offset_entry {
  double timestamp;
  double offset_hz;
};

struct freq_offset_t {
  struct freq_offset_entry *entries;
  size_t entries_len;
  // index of the interval [entries[current_index], entries[current_index + 1]] virtual_time currently falls in
  size_t current_index;

  // clock driven by processed samples (virtual_time += 1/sample_rate per sample), never by C time functions,
  // so that it advances in lockstep with the SDR clock rather than the wall clock
  double virtual_time;
  uint64_t sample_rate;

  // largest number of samples that can be mixed with a single frequency/phase increment: derived from
  // the smallest gap between consecutive entries' timestamps, so a batch never straddles an interval
  // where the offset should have changed
  size_t max_batch_samples;

  nco_crcf nco;

  float complex *output;
  size_t output_len;
};

static int freq_offset_load_file(const char *file_path, struct freq_offset_entry **entries, size_t *entries_len) {
  FILE *file = fopen(file_path, "r");
  if (file == NULL) {
    fprintf(stderr, "<3>unable to open freq offset file: %s\n", file_path);
    return -1;
  }

  size_t capacity = 16;
  size_t len = 0;
  struct freq_offset_entry *result = malloc(sizeof(struct freq_offset_entry) * capacity);
  if (result == NULL) {
    fclose(file);
    return -ENOMEM;
  }

  double timestamp;
  double offset_hz;
  while (fscanf(file, "%lf %lf", &timestamp, &offset_hz) == 2) {
    if (len == capacity) {
      capacity *= 2;
      struct freq_offset_entry *resized = realloc(result, sizeof(struct freq_offset_entry) * capacity);
      if (resized == NULL) {
        free(result);
        fclose(file);
        return -ENOMEM;
      }
      result = resized;
    }
    result[len].timestamp = timestamp;
    result[len].offset_hz = offset_hz;
    len++;
  }
  fclose(file);

  if (len == 0) {
    fprintf(stderr, "<3>freq offset file is empty or malformed: %s\n", file_path);
    free(result);
    return -EINVAL;
  }

  *entries = result;
  *entries_len = len;
  return 0;
}

static double freq_offset_now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double) tv.tv_sec + (double) tv.tv_usec / 1e6;
}

int freq_offset_create(const char *file_path, uint64_t sample_rate, size_t max_input_buffer_length, freq_offset **result_out) {
  struct freq_offset_t *result = malloc(sizeof(struct freq_offset_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct freq_offset_t){0};
  result->sample_rate = sample_rate;

  int code = freq_offset_load_file(file_path, &result->entries, &result->entries_len);
  if (code != 0) {
    freq_offset_destroy(result);
    return code;
  }

  if (result->entries[0].timestamp == 0.0) {
    // pre-recorded I/Q stream: ignore wall-clock time entirely
    result->current_index = 0;
    result->virtual_time = 0.0;
  } else {
    // one-time wall-clock read to locate the applicable interval; afterwards virtual_time is only
    // advanced by processed samples (see freq_offset_process)
    double now = freq_offset_now();
    size_t idx = 0;
    while (idx + 1 < result->entries_len && result->entries[idx + 1].timestamp <= now) {
      idx++;
    }
    if (idx == result->entries_len - 1 && result->entries[idx].timestamp < now) {
      fprintf(stderr, "<3>freq offset file %s has no entries applicable to the current time\n", file_path);
      freq_offset_destroy(result);
      return -EINVAL;
    }
    result->current_index = idx;
    result->virtual_time = now;
  }

  result->max_batch_samples = SIZE_MAX;
  if (result->entries_len > 1) {
    double min_dt = -1.0;
    for (size_t i = 0; i + 1 < result->entries_len; i++) {
      double dt = result->entries[i + 1].timestamp - result->entries[i].timestamp;
      if (dt < 0.0) {
        dt = 0.0;
      }
      if (min_dt < 0.0 || dt < min_dt) {
        min_dt = dt;
      }
    }
    size_t batch = (size_t) (min_dt * (double) result->sample_rate);
    result->max_batch_samples = (batch == 0) ? 1 : batch;
  }

  result->nco = nco_crcf_create(LIQUID_NCO);
  if (result->nco == NULL) {
    freq_offset_destroy(result);
    return -EINVAL;
  }

  result->output_len = max_input_buffer_length;
  result->output = malloc(sizeof(float complex) * result->output_len);
  if (result->output == NULL) {
    freq_offset_destroy(result);
    return -ENOMEM;
  }

  *result_out = result;
  return 0;
}

static double freq_offset_interpolate(freq_offset *fo) {
  if (fo->virtual_time <= fo->entries[0].timestamp) {
    return fo->entries[0].offset_hz;
  }
  if (fo->current_index >= fo->entries_len - 1) {
    return fo->entries[fo->entries_len - 1].offset_hz;
  }
  const struct freq_offset_entry *from = &fo->entries[fo->current_index];
  const struct freq_offset_entry *to = &fo->entries[fo->current_index + 1];
  double fraction = (fo->virtual_time - from->timestamp) / (to->timestamp - from->timestamp);
  return from->offset_hz + fraction * (to->offset_hz - from->offset_hz);
}

static void freq_offset_advance(freq_offset *fo) {
  while (fo->current_index + 1 < fo->entries_len && fo->virtual_time >= fo->entries[fo->current_index + 1].timestamp) {
    fo->current_index++;
  }
}

void freq_offset_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len, freq_offset *fo) {
  if (input_len > fo->output_len) {
    fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", input_len, fo->output_len);
    *output = NULL;
    *output_len = 0;
    return;
  }
  size_t processed = 0;
  while (processed < input_len) {
    size_t chunk_len = input_len - processed;
    if (chunk_len > fo->max_batch_samples) {
      chunk_len = fo->max_batch_samples;
    }

    // one frequency/phase increment per chunk: mix_block_* steps the nco's internal phase for every
    // sample as it mixes, so a single set_frequency call per chunk is enough. Chunks are capped at
    // max_batch_samples so the offset doesn't drift within a chunk across an interval boundary.
    double offset_hz = freq_offset_interpolate(fo);
    float dtheta = (float) (2.0 * M_PI * fabs(offset_hz) / (double) fo->sample_rate);
    nco_crcf_set_frequency(fo->nco, dtheta);
    if (offset_hz >= 0.0) {
      nco_crcf_mix_block_up(fo->nco, (float complex *) (input + processed), fo->output + processed, (unsigned int) chunk_len);
    } else {
      nco_crcf_mix_block_down(fo->nco, (float complex *) (input + processed), fo->output + processed, (unsigned int) chunk_len);
    }

    fo->virtual_time += (double) chunk_len / (double) fo->sample_rate;
    freq_offset_advance(fo);

    processed += chunk_len;
  }

  *output = fo->output;
  *output_len = input_len;
}

void freq_offset_destroy(freq_offset *fo) {
  if (fo == NULL) {
    return;
  }
  if (fo->entries != NULL) {
    free(fo->entries);
  }
  if (fo->nco != NULL) {
    nco_crcf_destroy(fo->nco);
  }
  if (fo->output != NULL) {
    free(fo->output);
  }
  free(fo);
}
