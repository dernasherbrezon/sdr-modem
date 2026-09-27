#ifndef DSP_HALFBAND_INTERP_H_
#define DSP_HALFBAND_INTERP_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct halfband_interp_t halfband_interp;

// multi-stage half-band interpolator (liquid-dsp msresamp2), interpolation factor is 2^num_stages.
// counterpart of halfband_decim: brings baseband samples back up to the raw sample rate
//  num_stages                : number of half-band interpolation stages, 0 < num_stages <= 16
//  fc                        : filter cut-off frequency normalized to the low (input) sample rate, 0 < fc < 0.5
//  stopband_attenuation_db   : stop-band attenuation in dB
int halfband_interp_create(unsigned int num_stages, float fc, float stopband_attenuation_db,
                           uint32_t max_input_buffer_length, halfband_interp **filter);

// output_len is always input_len * 2^num_stages
void halfband_interp_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len,
                             halfband_interp *filter);

size_t halfband_interp_max_output_buffer_length(halfband_interp *filter);

void halfband_interp_destroy(halfband_interp *filter);

#endif /* DSP_HALFBAND_INTERP_H_ */
