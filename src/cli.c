#include "cli.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "dsp/modem.h"
#include "sdr/file_source.h"
#include "sdr/plutosdr.h"
#include "sdr/sdr_device.h"
#include "sdr/sdr_server_client.h"

struct cli_t {
  sdr_device *rx_device;
  sdr_modem *rx_modem;
  FILE *output_file;

  sdr_device *tx_device;
  sdr_modem *tx_modem;
  FILE *input_file;
  uint8_t *input_temp;
  size_t input_temp_size;

  volatile sig_atomic_t do_exit;
};

static int cli_create_rx_sdr(app_config *config, struct cli_t *result) {
  if (config->rx_sdr_type == SDR_TYPE_SDR_SERVER) {
    struct sdr_rx rx = {
      .rx_center_freq = config->rx_req.gfsk->center_freq,
      .rx_sample_rate = config->rx_req.gfsk->sample_rate
    };
    int code = sdr_server_client_create(1, &rx, config->rx_sdr_server_address, config->rx_sdr_server_port, config->read_timeout_seconds, config->buffer_size, &result->rx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_PLUTOSDR) {
    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
    if (rx_config == NULL) {
      return -1;
    }
    rx_config->sample_rate = config->rx_req.gfsk->sample_rate;
    rx_config->center_freq = config->rx_req.gfsk->center_freq;
    rx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    rx_config->manual_gain = config->rx_plutosdr_gain;
    int code = plutosdr_create(1, config->tx_sdr_type == SDR_TYPE_PLUTOSDR, rx_config, NULL, config->tx_plutosdr_timeout_millis, config->buffer_size, config->iio, &result->rx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_FILE) {
    int code = file_source_create(1, config->rx_file, config->rx_file_format, NULL, config->tx_file_format, config->rx_req.gfsk->sample_rate, config->buffer_size, &result->rx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_NONE) {
    //do nothing, but supported
  } else {
    fprintf(stderr, "<3>unsupported rx_sdr_type: %d\n", config->rx_sdr_type);
    return -1;
  }
  return 0;
}

static int cli_create_tx_sdr(app_config *config, struct cli_t *result) {
  size_t max_modulation_buffer_length = modem_max_modulation_buffer_length(result->tx_modem);
  if (config->tx_sdr_type == SDR_TYPE_PLUTOSDR) {
    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
    if (tx_config == NULL) {
      return -1;
    }
    tx_config->sample_rate = config->tx_req.gfsk->sample_rate;
    tx_config->center_freq = config->tx_req.gfsk->center_freq;
    tx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    tx_config->manual_gain = config->tx_plutosdr_gain;
    int code = plutosdr_create(1, false, NULL, tx_config, config->tx_plutosdr_timeout_millis, max_modulation_buffer_length, config->iio, &result->tx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->tx_sdr_type == SDR_TYPE_FILE) {
    int code = file_source_create(1, NULL, config->rx_file_format, config->tx_file, config->tx_file_format, config->tx_req.gfsk->sample_rate, max_modulation_buffer_length, &result->tx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->tx_sdr_type == SDR_TYPE_NONE) {
    //do nothing, but supported
  } else {
    fprintf(stderr, "<3>unsupported rx_sdr_type: %d\n", config->rx_sdr_type);
    return -1;
  }
  return 0;
}

static int cli_create_rx_modem(app_config *config, struct cli_t *result) {
  return modem_create(config, &config->rx_req, config->rx_freq_offset_file, config->rx_debug_freq_offset_file, config->rx_debug_constellation_file, config->rx_debug_baseband_file, &result->rx_modem);
}

static int cli_create_tx_modem(app_config *config, struct cli_t *result) {
  return modem_create(config, &config->tx_req, config->tx_freq_offset_file, config->tx_debug_freq_offset_file, config->tx_debug_constellation_file, NULL, &result->tx_modem);
}

int cli_create(app_config *config, cli **output) {
  struct cli_t *result = malloc(sizeof(struct cli_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct cli_t){0};
  result->do_exit = 0;
  int code = cli_create_rx_sdr(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  code = cli_create_rx_modem(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  if (config->rx_sdr_type != SDR_TYPE_NONE) {
    result->output_file = fopen(config->output_file, "wb");
    if (result->output_file == NULL) {
      fprintf(stderr, "<3>unable to open file %s: %s\n", config->output_file, strerror(errno));
      cli_destroy(result);
      return -1;
    }
  }

  code = cli_create_tx_modem(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  code = cli_create_tx_sdr(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  if (config->tx_sdr_type != SDR_TYPE_NONE) {
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
  // one more safety
  if (cli->tx_modem != NULL && cli->tx_device != NULL && cli->input_file != NULL) {
    while (!cli->do_exit) {
      size_t actually_read = fread(cli->input_temp, sizeof(uint8_t), cli->input_temp_size, cli->input_file);
      if (actually_read < cli->input_temp_size) {
        break;
      }
      float complex *output = NULL;
      size_t output_len = 0;
      modem_modulate(cli->input_temp, cli->input_temp_size, &output, &output_len, cli->tx_modem);
      int code = cli->tx_device->sdr_process_tx(output, output_len, cli->tx_device->plugin);
      if (code != 0) {
        break;
      }
    }
  }

  if (cli->rx_modem != NULL && cli->rx_device != NULL && cli->output_file != NULL) {
    while (!cli->do_exit) {
      float complex *output = NULL;
      size_t output_len = 0;
      int code = cli->rx_device->sdr_process_rx(&output, &output_len, cli->rx_device->plugin);
      if (code != 0) {
        break;
      }
      int8_t *demodulated = NULL;
      size_t demodulated_len = 0;
      modem_demodulate(output, output_len, &demodulated, &demodulated_len, cli->rx_modem);
      size_t actually_written = fwrite(demodulated, sizeof(int8_t), demodulated_len, cli->output_file);
      if (actually_written != demodulated_len) {
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
  if (cli->rx_device != NULL) {
    cli->rx_device->stop_rx(cli->rx_device->plugin);
  }
}

void cli_destroy(cli *cli) {
  if (cli == NULL) {
    return;
  }
  if (cli->rx_device != NULL) {
    cli->rx_device->destroy(cli->rx_device->plugin);
    free(cli->rx_device);
  }
  if (cli->tx_device != NULL) {
    cli->tx_device->destroy(cli->tx_device->plugin);
    free(cli->tx_device);
  }
  if (cli->output_file != NULL) {
    fclose(cli->output_file);
  }
  if (cli->input_file != NULL) {
    fclose(cli->input_file);
  }
  if (cli->rx_modem != NULL) {
    modem_destroy(cli->rx_modem);
  }
  if (cli->tx_modem != NULL) {
    modem_destroy(cli->tx_modem);
  }
  if (cli->input_temp != NULL) {
    free(cli->input_temp);
  }
  free(cli);
}
