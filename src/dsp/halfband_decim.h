#ifndef DSP_HALFBAND_DECIM_H_
#define DSP_HALFBAND_DECIM_H_

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct halfband_decim_t halfband_decim;

// multi-stage half-band decimator (liquid-dsp msresamp2), decimation factor is 2^num_stages
//  num_stages                : number of half-band decimation stages, 0 < num_stages <= 16
//  fc                        : filter cut-off frequency normalized to the decimated (output) sample rate, 0 < fc < 0.5
//  stopband_attenuation_db   : stop-band attenuation in dB
int halfband_decim_create(unsigned int num_stages, float fc, float stopband_attenuation_db,
                          uint32_t max_input_buffer_length, halfband_decim **filter);

void halfband_decim_process(const float complex *input, size_t input_len, float complex **output, size_t *output_len,
                            halfband_decim *filter);

void halfband_decim_destroy(halfband_decim *filter);

#endif /* DSP_HALFBAND_DECIM_H_ */
