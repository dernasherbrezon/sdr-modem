#ifndef SDR_MODEM_GFSK_MOD_H
#define SDR_MODEM_GFSK_MOD_H

#include <stdlib.h>
#include <stdint.h>
#include <complex.h>

typedef struct gfsk_mod_t gfsk_mod;

int gfsk_mod_create(float samplesPerSymbol, float sensitivity, float bt, uint32_t max_input_buffer_length, gfsk_mod **mod);

void gfsk_mod_process(uint8_t *input, size_t input_len, float complex **output, size_t *output_len, gfsk_mod *mod);

void gfsk_mod_destroy(gfsk_mod *mod);

#endif //SDR_MODEM_GFSK_MOD_H
