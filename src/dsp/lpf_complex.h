#ifndef DSP_LPF_COMPLEX_H_
#define DSP_LPF_COMPLEX_H_

#include <stdint.h>
#include <complex.h>

typedef struct lpf_complex_t lpf_complex;

int lpf_complex_create(uint32_t sampling_freq, uint32_t cutoff_freq, uint32_t transition_width, size_t output_len, lpf_complex **filter);

void lpf_complex_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len, lpf_complex *filter);

int lpf_complex_destroy(lpf_complex *filter);

#endif /* DSP_LPF_COMPLEX_H_ */
