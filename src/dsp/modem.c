#include "modem.h"
#include <errno.h>
#include <stdio.h>
#include "gfsk_modem.h"

int modem_create(app_config *config, struct ModemRequest *req, const char *freq_offset_file, const char *debug_freq_offset_file, sdr_modem **modem) {
  if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS__NOT_SET) {
    //do nothing, but supported
    *modem = NULL;
    return 0;
  }

  struct sdr_modem_t *result = malloc(sizeof(struct sdr_modem_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_modem_t){0};
  int code = 0;
  uint64_t sample_rate = 0;
  if (req->modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_GFSK) {
    sample_rate = req->gfsk->sample_rate;
    code = gfsk_modem_create(req->gfsk, config->buffer_size, (gfsk_modem **) &result->modem);
    result->modulate = gfsk_modem_modulate;
    result->demodulate = gfsk_modem_demodulate;
    result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
    result->destroy = gfsk_modem_destroy;
  } else {
    fprintf(stderr, "<3>unsupported modem type: %d\n", req->modem_settings_case);
    code = -1;
  }

  if (code != 0) {
    free(result);
    return code;
  }

  if (freq_offset_file != NULL) {
    // needs to fit both the raw rx buffer and the (typically larger) modulated tx buffer, since
    // this same instance can end up being used for either direction
    size_t max_buffer_length = config->buffer_size;
    size_t max_modulation_buffer_length = result->max_modulation_buffer_length(result->modem);
    if (max_modulation_buffer_length > max_buffer_length) {
      max_buffer_length = max_modulation_buffer_length;
    }
    code = freq_offset_create(freq_offset_file, sample_rate, max_buffer_length, &result->freq_offset);
    if (code != 0) {
      modem_destroy(result);
      return code;
    }
  }

  if (debug_freq_offset_file != NULL) {
    result->debug_freq_offset_file = fopen(debug_freq_offset_file, "wb");
    if (result->debug_freq_offset_file == NULL) {
      fprintf(stderr, "<3>unable to open debug freq offset file: %s\n", debug_freq_offset_file);
      modem_destroy(result);
      return -1;
    }
  }

  *modem = result;
  return 0;
}

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
  if (modem->debug_freq_offset_file != NULL && *output != NULL) {
    fwrite(*output, sizeof(float complex), *output_len, modem->debug_freq_offset_file);
  }
  if (modem->freq_offset != NULL && *output != NULL) {
    freq_offset_process(*output, *output_len, output, output_len, modem->freq_offset);
  }
}

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem) {
  if (modem->freq_offset != NULL) {
    float complex *corrected = NULL;
    size_t corrected_len = 0;
    freq_offset_process(input, input_len, &corrected, &corrected_len, modem->freq_offset);
    input = corrected;
    input_len = corrected_len;
  }
  if (modem->debug_freq_offset_file != NULL) {
    fwrite(input, sizeof(float complex), input_len, modem->debug_freq_offset_file);
  }
  modem->demodulate(input, input_len, output, output_len, modem->modem);
}

size_t modem_max_modulation_buffer_length(sdr_modem *modem) {
  if (modem == NULL) {
    return 0;
  }
  return modem->max_modulation_buffer_length(modem->modem);
}

void modem_destroy(sdr_modem *modem) {
  if (modem == NULL) {
    return;
  }
  modem->destroy(modem->modem);
  if (modem->freq_offset != NULL) {
    freq_offset_destroy(modem->freq_offset);
  }
  if (modem->debug_freq_offset_file != NULL) {
    fclose(modem->debug_freq_offset_file);
  }
  free(modem);
}
