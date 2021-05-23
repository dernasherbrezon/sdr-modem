#include "gaussian_taps.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int gaussian_taps_create(double gain, double samples_per_symbol, double bt, size_t taps_len, float **taps) {
    float *result = malloc(sizeof(float) * taps_len);
    if (result == NULL) {
        return -ENOMEM;
    }

    double scale = 0;
    double dt = 1.0 / samples_per_symbol;
    double s = 1.0 / (sqrt(log(2.0)) / (2 * M_PI * bt));
    double t0 = -0.5 * taps_len;
    double ts;
    for (int i = 0; i < taps_len; i++) {
        t0++;
        ts = s * dt * t0;
        result[i] = exp(-0.5 * ts * ts);
        scale += result[i];
    }
    for (int i = 0; i < taps_len; i++) {
        result[i] = result[i] / scale * gain;
    }

    *taps = result;
    return 0;
}