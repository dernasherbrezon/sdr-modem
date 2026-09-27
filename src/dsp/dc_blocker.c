#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

#include "dc_blocker.h"

// linear-phase dc removal filter described by R. Lyons, "Linear-phase DC Removal Filter", dsprelated.com, 2008:
// the dc estimate is 4 cascaded boxcar averages of the input, and it is subtracted from the input delayed
// by the group delay of that cascade, so the passband has no phase distortion
#define DC_BLOCKER_STAGES 4

struct boxcar {
  // last "length" inputs, oldest at index
  float *samples;
  size_t index;
  float sum;
};

struct dc_blocker_t {
  size_t length;
  struct boxcar stages[DC_BLOCKER_STAGES];

  // input delayed by the group delay of the cascade: DC_BLOCKER_STAGES * (length - 1) / 2
  float *delayed;
  size_t delayed_len;
  size_t delayed_index;
};

static float boxcar_process(float input, size_t length, struct boxcar *stage) {
  // running sum: add the newest sample, remove the one that just left the window
  stage->sum += input - stage->samples[stage->index];
  stage->samples[stage->index] = input;
  stage->index++;
  if (stage->index == length) {
    stage->index = 0;
  }
  return stage->sum / (float) length;
}

int dc_blocker_create(int length, dc_blocker **blocker) {
  if (length < 2) {
    return -EINVAL;
  }
  struct dc_blocker_t *result = malloc(sizeof(struct dc_blocker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct dc_blocker_t) {0};
  result->length = (size_t) length;

  for (int i = 0; i < DC_BLOCKER_STAGES; i++) {
    result->stages[i].samples = calloc(result->length, sizeof(float));
    if (result->stages[i].samples == NULL) {
      dc_blocker_destroy(result);
      return -ENOMEM;
    }
  }

  result->delayed_len = DC_BLOCKER_STAGES * (result->length - 1) / 2;
  result->delayed = calloc(result->delayed_len, sizeof(float));
  if (result->delayed == NULL) {
    dc_blocker_destroy(result);
    return -ENOMEM;
  }

  *blocker = result;
  return 0;
}

void dc_blocker_process(float *input, size_t input_len, float **output, size_t *output_len, dc_blocker *blocker) {
  for (size_t i = 0; i < input_len; i++) {
    float dc = input[i];
    for (int j = 0; j < DC_BLOCKER_STAGES; j++) {
      dc = boxcar_process(dc, blocker->length, &blocker->stages[j]);
    }

    float delayed = blocker->delayed[blocker->delayed_index];
    blocker->delayed[blocker->delayed_index] = input[i];
    blocker->delayed_index++;
    if (blocker->delayed_index == blocker->delayed_len) {
      blocker->delayed_index = 0;
    }

    input[i] = delayed - dc;
  }
  *output = input;
  *output_len = input_len;
}

void dc_blocker_destroy(dc_blocker *blocker) {
  if (blocker == NULL) {
    return;
  }
  for (int i = 0; i < DC_BLOCKER_STAGES; i++) {
    if (blocker->stages[i].samples != NULL) {
      free(blocker->stages[i].samples);
    }
  }
  if (blocker->delayed != NULL) {
    free(blocker->delayed);
  }
  free(blocker);
}
