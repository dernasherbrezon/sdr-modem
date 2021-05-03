#ifndef SDR_MODEM_MMSE_FIR_INTERPOLATOR_H
#define SDR_MODEM_MMSE_FIR_INTERPOLATOR_H

#include <stdlib.h>

typedef struct mmse_fir_interpolator_t mmse_fir_interpolator;

int mmse_fir_interpolator_create(mmse_fir_interpolator **interp);

float mmse_fir_interpolator_process(const float *input, float mu, mmse_fir_interpolator *interp);

void mmse_fir_interpolator_destroy(mmse_fir_interpolator *interp);

int mmse_fir_interpolator_taps(mmse_fir_interpolator *interp);

#endif //SDR_MODEM_MMSE_FIR_INTERPOLATOR_H
