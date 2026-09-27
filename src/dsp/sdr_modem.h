#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../api.pb-c.h"
#include "../app_config.h"
#include "freq_offset.h"

typedef struct sdr_modem_t sdr_modem;

// freq_offset_file may be NULL, in which case no frequency correction is applied
int sdr_modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, sdr_modem **modem);

// all debug setters below accept NULL as the file name, which closes and disables the dump.
// they are no-ops returning 0 if modem is NULL. return non-zero if the file cannot be opened.
// dumps raw I/Q, see debug_freq_offset_file
int sdr_modem_set_debug_freq_offset_file(const char *debug_freq_offset_file, sdr_modem *modem);

// rx only. dumps baseband I/Q, see debug_baseband_file
int sdr_modem_set_debug_baseband_file(const char *debug_baseband_file, sdr_modem *modem);

// only honored by bpsk/dpsk/sdpsk/oqpsk/psk_pm modems, ignored (returns 0) for others
int sdr_modem_set_debug_constellation_file(const char *debug_constellation_file, sdr_modem *modem);

// rx only. only honored by the psk_pm modem, ignored (returns 0) for others -- see psk_pm_modem.h
int sdr_modem_set_debug_subcarrier_file(const char *debug_subcarrier_file, sdr_modem *modem);

void sdr_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void sdr_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t sdr_modem_max_modulation_buffer_length(sdr_modem *modem);

void sdr_modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
