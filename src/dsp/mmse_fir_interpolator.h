#ifndef SDR_MODEM_MMSE_FIR_INTERPOLATOR_H
#define SDR_MODEM_MMSE_FIR_INTERPOLATOR_H

#include <stdlib.h>

typedef struct mmse_fir_interpolator_t mmse_fir_interpolator;

int mmse_fir_interpolator_create(size_t output_len, mmse_fir_interpolator **interp);

void mmse_fir_interpolator_process(const float *input, const size_t input_len, float mu, mmse_fir_interpolator *interp);

void mmse_fir_interpolator_destroy(mmse_fir_interpolator *interp);

#endif //SDR_MODEM_MMSE_FIR_INTERPOLATOR_H
