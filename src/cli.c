#include "cli.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "dsp/sdr_modem.h"
#include "sdr/sdr_file.h"
#include "sdr/plutosdr.h"
#include "sdr/sdr_device.h"
#include "sdr/sdr_server_client.h"

struct cli_t {
  int direction;

  sdr_device *device;
  sdr_modem *modem;

  FILE *output_file;

  FILE *input_file;
  uint8_t *input_temp;
  size_t input_temp_size;

  volatile sig_atomic_t do_exit;
};

static int cli_create_sdr(app_config *config, struct cli_t *result) {
  if (config->sdr_type == SDR_TYPE_SDR_SERVER) {
    sdr_server_settings settings = config->sdr_server;
    settings.frequency = config->frequency;
    settings.sample_rate = config->sample_rate;
    int code = sdr_server_client_create(1, &settings, config->buffer_size, &result->device);
    if (code != 0) {
      return -1;
    }
  } else if (config->sdr_type == SDR_TYPE_PLUTOSDR) {
    plutosdr_settings settings = {
        .timeout_ms = config->plutosdr_timeout_millis
    };
    int code = 0;
    if (config->direction == DIRECTION_RX) {
      // cli works in a single direction. tx can be safely disabled
      settings.rx_only = true;
      settings.rx_sample_rate = config->sample_rate;
      settings.rx_center_freq = config->frequency;
      settings.rx_gain_control_mode = IIO_GAIN_MODE_MANUAL;
      settings.rx_manual_gain = config->plutosdr_gain;
      code = plutosdr_create(1, &settings, config->buffer_size, config->iio, &result->device);
    } else {
      settings.rx_only = false;
      settings.tx_sample_rate = config->sample_rate;
      settings.tx_center_freq = config->frequency;
      settings.tx_gain_control_mode = IIO_GAIN_MODE_MANUAL;
      settings.tx_manual_gain = config->plutosdr_gain;
      size_t max_modulation_buffer_length = sdr_modem_max_modulation_buffer_length(result->modem);
      code = plutosdr_create(1, &settings, max_modulation_buffer_length, config->iio, &result->device);
    }
    if (code != 0) {
      return -1;
    }
  } else if (config->sdr_type == SDR_TYPE_FILE) {
    sdr_file_settings settings = config->sdr_file;
    settings.frequency = config->frequency;
    settings.sample_rate = config->sample_rate;
    int code = 0;
    // cli works in a single direction. do not open (and truncate) tx_file during rx
    if (config->direction == DIRECTION_RX) {
      settings.tx_file = NULL;
      code = sdr_file_create(1, &settings, config->buffer_size, &result->device);
    } else {
      settings.rx_file = NULL;
      size_t max_modulation_buffer_length = sdr_modem_max_modulation_buffer_length(result->modem);
      code = sdr_file_create(1, &settings, max_modulation_buffer_length, &result->device);
    }
    if (code != 0) {
      return -1;
    }
  } else {
    fprintf(stderr, "<3>unsupported sdr_type: %d\n", config->sdr_type);
    return -1;
  }
  return 0;
}

static int cli_create_modem(app_config *config, struct cli_t *result) {
  sdr_modem_settings settings;
  switch (config->modem) {
    case MODEM_TYPE_GFSK:
      settings.gfsk = config->gfsk;
      settings.gfsk.sample_rate = config->sample_rate;
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      settings.psk = config->psk;
      settings.psk.sample_rate = config->sample_rate;
      break;
    case MODEM_TYPE_PSK_PM:
      settings.psk_pm = config->psk_pm;
      settings.psk_pm.sample_rate = config->sample_rate;
      break;
    default:
      fprintf(stderr, "<3>unsupported modem type: %d\n", config->modem);
      return -1;
  }
  int code = sdr_modem_create(config->modem, &settings, config->buffer_size, config->freq_offset_file, &result->modem);
  if (code != 0) {
    return code;
  }
  code = sdr_modem_set_debug_freq_offset_file(config->debug_freq_offset_file, result->modem);
  if (code != 0) {
    return code;
  }
  code = sdr_modem_set_debug_constellation_file(config->debug_constellation_file, result->modem);
  if (code != 0) {
    return code;
  }
  if (config->direction == DIRECTION_RX) {
    code = sdr_modem_set_debug_baseband_file(config->debug_baseband_file, result->modem);
    if (code != 0) {
      return code;
    }
    code = sdr_modem_set_debug_subcarrier_file(config->debug_subcarrier_file, result->modem);
    if (code != 0) {
      return code;
    }
  }
  return 0;
}

int cli_create(app_config *config, cli **output) {
  struct cli_t *result = malloc(sizeof(struct cli_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct cli_t){0};
  result->do_exit = 0;
  result->direction = config->direction;

  int code = cli_create_modem(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  code = cli_create_sdr(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  if (config->direction == DIRECTION_RX) {
    result->output_file = fopen(config->output_file, "wb");
    if (result->output_file == NULL) {
      fprintf(stderr, "<3>unable to open file %s: %s\n", config->output_file, strerror(errno));
      cli_destroy(result);
      return -1;
    }
  } else {
    result->input_file = fopen(config->input_file, "rb");
    if (result->input_file == NULL) {
      fprintf(stderr, "<3>unable to open file %s: %s\n", config->input_file, strerror(errno));
      cli_destroy(result);
      return -1;
    }
    result->input_temp_size = config->buffer_size;
    result->input_temp = malloc(sizeof(uint8_t) * result->input_temp_size);
    if (result->input_temp == NULL) {
      cli_destroy(result);
      return -1;
    }
  }

  *output = result;
  return 0;
}

int cli_process(cli *cli) {
  if (cli->direction == DIRECTION_RX) {
    while (!cli->do_exit) {
      float complex *output = NULL;
      size_t output_len = 0;
      int code = cli->device->sdr_process_rx(&output, &output_len, cli->device->plugin);
      if (code != 0) {
        break;
      }
      int8_t *demodulated = NULL;
      size_t demodulated_len = 0;
      sdr_modem_demodulate(output, output_len, &demodulated, &demodulated_len, cli->modem);
      size_t actually_written = fwrite(demodulated, sizeof(int8_t), demodulated_len, cli->output_file);
      if (actually_written != demodulated_len) {
        break;
      }
    }
  } else {
    while (!cli->do_exit) {
      size_t actually_read = fread(cli->input_temp, sizeof(uint8_t), cli->input_temp_size, cli->input_file);
      if (actually_read < cli->input_temp_size) {
        break;
      }
      float complex *output = NULL;
      size_t output_len = 0;
      sdr_modem_modulate(cli->input_temp, cli->input_temp_size, &output, &output_len, cli->modem);
      int code = cli->device->sdr_process_tx(output, output_len, cli->device->plugin);
      if (code != 0) {
        break;
      }
    }
  }
  return 0;
}

void cli_stop(cli *cli) {
  if (cli == NULL) {
    return;
  }
  cli->do_exit = 1;
  if (cli->direction == DIRECTION_RX && cli->device != NULL) {
    cli->device->stop_rx(cli->device->plugin);
  }
}

void cli_destroy(cli *cli) {
  if (cli == NULL) {
    return;
  }
  if (cli->device != NULL) {
    cli->device->destroy(cli->device->plugin);
    free(cli->device);
  }
  if (cli->output_file != NULL) {
    fclose(cli->output_file);
  }
  if (cli->input_file != NULL) {
    fclose(cli->input_file);
  }
  if (cli->modem != NULL) {
    sdr_modem_destroy(cli->modem);
  }
  if (cli->input_temp != NULL) {
    free(cli->input_temp);
  }
  free(cli);
}
