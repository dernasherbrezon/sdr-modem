#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "freq_offset.h"
#include "gfsk_modem.h"
#include "bpsk_modem.h"
#include "psk_pm_modem.h"

typedef enum {
  // returned when a modem name cannot be parsed
  MODEM_TYPE_INVALID = -1,
  MODEM_TYPE_NONE = 0,
  MODEM_TYPE_GFSK,
  MODEM_TYPE_BPSK,
  MODEM_TYPE_DPSK,
  MODEM_TYPE_SDPSK,
  MODEM_TYPE_PSK_PM
} sdr_modem_type;

typedef struct sdr_modem_t sdr_modem;

// modem-specific settings. the active member is selected by the MODEM_TYPE_* passed along with it
typedef union {
  gfsk_modem_settings gfsk;
  // bpsk, dpsk and sdpsk share the same settings. psk.type must match the modem type
  bpsk_modem_settings psk;
  psk_pm_modem_settings psk_pm;
} sdr_modem_settings;

int sdr_modem_create(sdr_modem_type modem_type, const sdr_modem_settings *settings, uint32_t buffer_size, const char *freq_offset_file, sdr_modem **modem);

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
