#include "gaussian_taps.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int gaussian_taps_create(float samples_per_symbol, float bt, size_t taps_len, float **taps) {
  float *result = malloc(sizeof(float) * taps_len);
  if (result == NULL) {
    return -ENOMEM;
  }

  float scale = 0;
  float dt = 1.0f / samples_per_symbol;
  float s = 1.0f / (sqrtf(logf(2.0f)) / (2 * (float) M_PI * bt));
  float t0 = -0.5f * (float) taps_len;
  for (size_t i = 0; i < taps_len; i++) {
    t0++;
    float ts = s * dt * t0;
    result[i] = expf(-0.5f * ts * ts);
    scale += result[i];
  }
  for (size_t i = 0; i < taps_len; i++) {
    result[i] = result[i] / scale;
  }

  *taps = result;
  return 0;
}
