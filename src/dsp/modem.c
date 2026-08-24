#include "modem.h"
#include <errno.h>
#include <stdio.h>
#include "gfsk_modem.h"

int modem_create(app_config *config, bool is_tx, sdr_modem **modem) {
  int modem_type = is_tx ? config->tx_modem : config->rx_modem;
  if (modem_type == MODEM_TYPE_NONE) {
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

  struct ModemRequest *req = is_tx ? &config->tx_req : &config->rx_req;

  int code = 0;
  if (modem_type == MODEM_TYPE_GFSK) {
    code = gfsk_modem_create(req->gfsk, config->buffer_size, (gfsk_modem **) &result->modem);
    result->modulate = gfsk_modem_modulate;
    result->demodulate = gfsk_modem_demodulate;
    result->max_modulation_buffer_length = gfsk_modem_max_modulation_buffer_length;
    result->destroy = gfsk_modem_destroy;
  } else {
    fprintf(stderr, "<3>unsupported modem type: %d\n", modem_type);
    code = -1;
  }

  if (code != 0) {
    free(result);
    return code;
  }

  *modem = result;
  return 0;
}

void modem_modulate(const uint8_t *input, size_t input_len, float complex **output, size_t *output_len, sdr_modem *modem) {
  modem->modulate(input, input_len, output, output_len, modem->modem);
}

void modem_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, sdr_modem *modem) {
  modem->demodulate(input, input_len, output, output_len, modem->modem);
}

size_t modem_max_modulation_buffer_length(sdr_modem *modem) {
  return modem->max_modulation_buffer_length(modem->modem);
}

void modem_destroy(sdr_modem *modem) {
  if (modem == NULL) {
    return;
  }
  modem->destroy(modem->modem);
  free(modem);
}
