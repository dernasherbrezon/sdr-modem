#ifndef DSP_CLOCK_RECOVERY_MM_H_
#define DSP_CLOCK_RECOVERY_MM_H_

#include <stdlib.h>

typedef struct clock_mm_t clock_mm;

int clock_mm_create(float omega, float gain_omega, float mu, float gain_mu, float omega_relative_limit, clock_mm **clock);

void clock_mm_process(const float *input, size_t input_len, float **output, size_t *output_len, clock_mm *clock);

void clock_mm_destroy(clock_mm *clock);

#endif /* DSP_CLOCK_RECOVERY_MM_H_ */
