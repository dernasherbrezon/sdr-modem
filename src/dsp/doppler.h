#ifndef SDR_MODEM_DOPPLER_H
#define SDR_MODEM_DOPPLER_H

#define _POSIX_C_SOURCE 200809L

#include <complex.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>


typedef struct doppler_t doppler;

int doppler_create(double latitude, double longitude, double altitude, uint64_t sampling_freq, uint64_t center_freq, int64_t constant_offset, time_t start_time_seconds, uint32_t max_output_buffer_length, char tle[3][80], doppler **result);

void doppler_process_rx(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result);

void doppler_process_tx(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result);

void doppler_destroy(doppler *result);

#endif //SDR_MODEM_DOPPLER_H
