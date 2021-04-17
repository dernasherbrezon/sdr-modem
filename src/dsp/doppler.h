#ifndef SDR_MODEM_DOPPLER_H
#define SDR_MODEM_DOPPLER_H

#include <complex.h>
#include <stdlib.h>

typedef struct doppler_t doppler;

int doppler_create(float latitude, float longitude, float altitude, uint32_t rx_sampling_freq, uint32_t rx_center_freq, uint32_t max_output_buffer_length, char tle[3][80], doppler **result);

void doppler_process(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result);

void doppler_destroy(doppler *result);

#endif //SDR_MODEM_DOPPLER_H
