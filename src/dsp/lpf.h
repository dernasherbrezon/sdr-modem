#ifndef DSP_LPF_H_
#define DSP_LPF_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct lpf_t lpf;

int lpf_create(uint8_t decimation, uint64_t sampling_freq, uint64_t cutoff_freq, uint32_t transition_width, size_t output_len, size_t num_bytes, lpf **filter);

void lpf_process(const void *input, size_t input_len, void **output, size_t *output_len, lpf *filter);

void lpf_destroy(lpf *filter);

#endif /* DSP_LPF_H_ */
