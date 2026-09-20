#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../api.pb-c.h"
#include "../app_config.h"
#include "freq_offset.h"
#include "halfband_decim.h"

typedef struct sdr_modem_t sdr_modem;

struct sdr_modem_t {
  void *modem;

  void (*modulate)(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, void *modem);

  void (*demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  size_t (*max_modulation_buffer_length)(void *modem);

  void (*destroy)(void *modem);

  // optional. decimates raw I/Q down to just above the signal bandwidth before demodulate, so the
  // wrapped modem's own DSP chain runs at a lower, cheaper sample rate
  halfband_decim *halfband;

  // optional. applied to raw I/Q before demodulate and to the modulated output before tx
  freq_offset *freq_offset;

  // optional. dumps raw I/Q samples for debugging: on rx, the (possibly freq_offset-corrected)
  // samples right before demodulate; on tx, the modulated samples right after modulate, before
  // freq_offset correction is applied
  FILE *debug_freq_offset_file;

  // optional. rx only: dumps the baseband I/Q samples right after halfband decimation
  // (i.e. right before the wrapped demodulator runs), or the freq_offset-corrected raw
  // input when no halfband decimation is configured
  FILE *debug_baseband_file;

  // which wrapped modem is in use, needed to dispatch modem-specific debug setters
  ModemRequest__ModemSettingsCase modem_settings_case;

  // sample rate seen by the wrapped modem, i.e. after halfband decimation
  uint64_t baseband_sample_rate;
};

// generic accessors for the fields common to every modem type (gfsk/bpsk/dpsk/sdpsk/oqpsk/psk_pm).
// return 0 if req->modem_settings_case is MODEM_REQUEST__MODEM_SETTINGS__NOT_SET or unrecognized.
uint64_t modem_request_get_center_freq(const struct ModemRequest *req);

uint64_t modem_request_get_sample_rate(const struct ModemRequest *req);

uint32_t modem_request_get_baud_rate(const struct ModemRequest *req);

// freq_offset_file may be NULL, in which case no frequency correction is applied
int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, sdr_modem **modem);

// all debug setters below accept NULL as the file name, which closes and disables the dump.
// they are no-ops returning 0 if modem is NULL. return non-zero if the file cannot be opened.
// dumps raw I/Q, see debug_freq_offset_file
int modem_set_debug_freq_offset_file(const char *debug_freq_offset_file, sdr_modem *modem);

// rx only. dumps baseband I/Q, see debug_baseband_file
int modem_set_debug_baseband_file(const char *debug_baseband_file, sdr_modem *modem);

// only honored by bpsk/dpsk/sdpsk/oqpsk/psk_pm modems, ignored (returns 0) for others
int modem_set_debug_constellation_file(const char *debug_constellation_file, sdr_modem *modem);

// rx only. only honored by the psk_pm modem, ignored (returns 0) for others -- see psk_pm_modem.h
int modem_set_debug_subcarrier_file(const char *debug_subcarrier_file, sdr_modem *modem);

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

size_t modem_max_modulation_buffer_length(sdr_modem *modem);

void modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
