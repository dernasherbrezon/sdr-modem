#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lpf_taps.h"
#include <inttypes.h>

int sanity_check_1f(uint64_t sampling_freq, uint64_t cutoff_freq, uint32_t transition_width) {
    if (sampling_freq <= 0) {
        fprintf(stderr, "<3>sampling frequency should be positive\n");
        return -1;
    }

    if (cutoff_freq <= 0 || (double) cutoff_freq > (double) sampling_freq / 2) {
        fprintf(stderr, "<3>cutoff frequency should be positive and less than sampling freq / 2. got: %" PRIu64 "\n", cutoff_freq);
        return -1;
    }

    if (transition_width <= 0) {
        fprintf(stderr, "<3>transition width should be positive\n");
        return -1;
    }

    return 0;
}

int computeNtaps(uint64_t sampling_freq, uint64_t transition_width) {
    double a = 53;
    int ntaps = (int) (a * (double) sampling_freq / (22.0 * (double) transition_width));
    if ((ntaps & 1) == 0) { // if even...
        ntaps++; // ...make odd
    }
    return ntaps;
}

int create_hamming_window(int ntaps, float **output) {
    float *result = malloc(sizeof(float) * ntaps);
    if (result == NULL) {
        return -ENOMEM;
    }
    int m = ntaps - 1;
    for (int n = 0; n < ntaps; n++) {
        result[n] = (float) (0.54 - 0.46 * cos((2 * M_PI * n) / m));
    }
    *output = result;
    return 0;
}

int create_low_pass_filter(float gain, uint64_t sampling_freq, uint64_t cutoff_freq, uint32_t transition_width, float **output_taps, size_t *len) {
    int code = sanity_check_1f(sampling_freq, cutoff_freq, transition_width);
    if (code != 0) {
        return code;
    }

    int ntaps = computeNtaps(sampling_freq, transition_width);
    float *taps = malloc(sizeof(float) * ntaps);
    if (taps == NULL) {
        return -ENOMEM;
    }
    memset(taps, 0, sizeof(float) * ntaps);

    float *w = NULL;
    code = create_hamming_window(ntaps, &w);
    if (code != 0) {
        free(taps);
        return code;
    }

    int M = (ntaps - 1) / 2;
    double fwT0 = 2 * M_PI * (double) cutoff_freq / (double) sampling_freq;

    for (int n = -M; n <= M; n++) {
        if (n == 0) {
            taps[n + M] = (float) (fwT0 / M_PI * w[n + M]);
        } else {
            // a little algebra gets this into the more familiar sin(x)/x form
            taps[n + M] = (float) (sin((double) n * fwT0) / (n * M_PI) * w[n + M]);
        }
    }

    free(w);

    float fmax = taps[0 + M];
    for (int n = 1; n <= M; n++) {
        fmax += 2 * taps[n + M];
    }

    gain /= fmax; // normalize

    for (int i = 0; i < ntaps; i++) {
        taps[i] *= gain;
    }

    *output_taps = taps;
    *len = ntaps;
    return 0;
}

