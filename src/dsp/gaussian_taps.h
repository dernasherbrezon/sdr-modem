#ifndef SDR_MODEM_GAUSSIAN_TAPS_H
#define SDR_MODEM_GAUSSIAN_TAPS_H

#include <stdlib.h>

int gaussian_taps_create(float samples_per_symbol, float bt, size_t taps_len, float **taps);

#endif //SDR_MODEM_GAUSSIAN_TAPS_H
