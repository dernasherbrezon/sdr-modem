#ifndef SDR_MODEM_GAUSSIAN_TAPS_H
#define SDR_MODEM_GAUSSIAN_TAPS_H

#include <stdlib.h>

int gaussian_taps_create(double gain, double samples_per_symbol, double bt, size_t taps_len, float **taps);

#endif //SDR_MODEM_GAUSSIAN_TAPS_H
