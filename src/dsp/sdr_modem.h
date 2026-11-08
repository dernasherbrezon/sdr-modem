#ifndef SDR_MODEM_MODEM_H
#define SDR_MODEM_MODEM_H

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "carrier_detector.h"
#include "freq_offset.h"
#include "gfsk_modem.h"
#include "bpsk_modem.h"
#include "psk_pm_modem.h"

typedef enum {
  // returned when a modem name cannot be parsed
  MODEM_TYPE_INVALID = 0,
  MODEM_TYPE_GFSK = 1,
  MODEM_TYPE_BPSK = 2,
  MODEM_TYPE_DPSK = 3,
  MODEM_TYPE_SDPSK = 4,
  MODEM_TYPE_PSK_PM = 5
} sdr_modem_type;

typedef struct sdr_modem_t sdr_modem;

typedef struct {
  uint8_t modem_type; // sdr_modem_type

  union {
    gfsk_modem_settings gfsk;
    // bpsk, dpsk and sdpsk share the same settings. psk.type must match the modem type
    bpsk_modem_settings psk;
    psk_pm_modem_settings psk_pm;
  } modem;
} sdr_modem_settings;


int sdr_modem_create(const sdr_modem_settings *settings, uint32_t buffer_size, const char *freq_offset_file, sdr_modem **modem);

// all debug setters below accept NULL as the file name, which closes and disables the dump.
// they are no-ops returning 0 if modem is NULL. return non-zero if the file cannot be opened.
// dumps raw I/Q, see debug_freq_offset_file
int sdr_modem_set_debug_freq_offset_file(const char *debug_freq_offset_file, sdr_modem *modem);

// rx only. dumps baseband I/Q as the wrapped modem gets it (after the carrier correction), see debug_baseband_file
int sdr_modem_set_debug_baseband_file(const char *debug_baseband_file, sdr_modem *modem);

// only honored by bpsk/dpsk/sdpsk/oqpsk/psk_pm modems, ignored (returns 0) for others
int sdr_modem_set_debug_constellation_file(const char *debug_constellation_file, sdr_modem *modem);

// rx only. only honored by the psk_pm modem, ignored (returns 0) for others -- see psk_pm_modem.h
int sdr_modem_set_debug_subcarrier_file(const char *debug_subcarrier_file, sdr_modem *modem);

// streaming: no tail is produced, the filters keep their history for the next call. call
// sdr_modem_modulate_flush at the end of the transmission
void sdr_modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem);

// assumes zero input and produces the remaining tail samples of what was modulated since the last flush,
// then resets the tx state (filters, differential reference, phases) so that the next sdr_modem_modulate starts a
// new independent transmission. output is NULL and output_len is 0 if there is nothing to flush.
// the output buffer is reused: consume the output of sdr_modem_modulate first
void sdr_modem_modulate_flush(float complex **output, size_t *output_len, sdr_modem *modem);

void sdr_modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem);

// rx only. carrier frequency offset of the received signal, in Hz (positive: the signal is above the center).
// 0 if modem is NULL or there is no estimate yet. it is what is left after the freq_offset (doppler) correction.
// it is the sum of two parts:
// - the frequency of the last detected carrier (see sdr_modem_set_carrier_callback). sdr_modem removes it from the baseband
//   samples before the wrapped modem, so that the modem starts the transmission close to the center
// - what the wrapped modem still tracks after that. what each modem can estimate differs:
//   see gfsk_modem_get_frequency_offset, bpsk_modem_get_frequency_offset and psk_pm_modem_get_frequency_offset
float sdr_modem_get_frequency_offset(sdr_modem *modem);

// rx only. modulation independent: every transmission is expected to start with an unmodulated carrier. the callback is
// called once per transmission, from the thread which calls sdr_modem_demodulate, with the coarse frequency offset of the carrier in Hz
// (positive: the carrier is above the center). like sdr_modem_get_frequency_offset it is what is left after the
// freq_offset (doppler) correction. the detector runs on the baseband samples, i.e. within the signal bandwidth
// after the half-band decimation. see carrier_detector.h. NULL callback disables the notification.
// the callback does not control the correction: sdr_modem always mixes the baseband down by the detected frequency
// before the wrapped modem, starting with the buffer which contains the carrier, and keeps doing it until the next
// carrier is detected (the new frequency replaces the old one, it does not add up). a new modem starts uncorrected
void sdr_modem_set_carrier_callback(carrier_detector_callback callback, void *ctx, sdr_modem *modem);

size_t sdr_modem_max_modulation_buffer_length(sdr_modem *modem);

void sdr_modem_destroy(sdr_modem *modem);


#endif //SDR_MODEM_MODEM_H
