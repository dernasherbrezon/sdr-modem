#ifndef SDR_MODEM_FREQUENCY_MODULATOR_H
#define SDR_MODEM_FREQUENCY_MODULATOR_H

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct frequency_modulator_t frequency_modulator;

int frequency_modulator_create(float sensitivity, uint32_t max_input_buffer_length, frequency_modulator **mod);

void frequency_modulator_process(float *input, size_t input_len, float complex **output, size_t *output_len, frequency_modulator *mod);

void frequency_modulator_destroy(frequency_modulator *mod);

#endif //SDR_MODEM_FREQUENCY_MODULATOR_H
