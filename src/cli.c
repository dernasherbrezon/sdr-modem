#include "cli.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "dsp/gfsk_modem.h"
#include "sdr/file_source.h"
#include "sdr/plutosdr.h"
#include "sdr/sdr_device.h"
#include "sdr/sdr_server_client.h"

struct cli_t {
  sdr_device *rx_device;
  FILE *output_file;
  void *rx_modem;

  void (*rx_modem_demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  void (*rx_modem_destroy)(void *modem);

  sdr_device *tx_device;
  FILE *input_file;
  void *tx_modem;

  void (*tx_modem_demodulate)(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, void *modem);

  void (*tx_modem_destroy)(void *modem);

  volatile sig_atomic_t do_exit;
};

static int cli_create_rx_sdr(app_config *config, struct cli_t *result) {
  if (config->rx_sdr_type == SDR_TYPE_SDR_SERVER) {
    struct sdr_rx rx = {
      .rx_center_freq = config->rx_req.gfsk->center_freq,
      .rx_offset = config->rx_req.gfsk->offset,
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
    rx_config->center_freq = config->rx_req.gfsk->center_freq + config->rx_req.gfsk->offset;
    rx_config->offset = config->rx_req.gfsk->offset;
    rx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    rx_config->manual_gain = config->rx_plutosdr_gain;
    int code = plutosdr_create(1, config->tx_sdr_type == SDR_TYPE_PLUTOSDR, rx_config, NULL, config->tx_plutosdr_timeout_millis, config->buffer_size, config->iio, &result->rx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_FILE) {
    int code = file_source_create(1, config->rx_file, NULL, config->rx_req.gfsk->sample_rate, config->rx_req.gfsk->offset, config->buffer_size, &result->rx_device);
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
  if (config->tx_sdr_type == SDR_TYPE_PLUTOSDR) {
    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
    if (tx_config == NULL) {
      return -1;
    }
    tx_config->sample_rate = config->tx_req.gfsk->sample_rate;
    tx_config->center_freq = config->tx_req.gfsk->center_freq;
    tx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    tx_config->manual_gain = config->tx_plutosdr_gain;
    int code = plutosdr_create(1, false, NULL, tx_config, config->tx_plutosdr_timeout_millis, config->buffer_size, config->iio, &result->tx_device);
    if (code != 0) {
      return -1;
    }
  } else if (config->tx_sdr_type == SDR_TYPE_FILE) {
    int code = file_source_create(1, config->tx_file, NULL, config->tx_req.gfsk->sample_rate, config->tx_req.gfsk->offset, config->buffer_size, &result->tx_device);
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
  if (config->rx_modem == MODEM_TYPE_GFSK) {
    int code = gfsk_modem_create(config->rx_req.gfsk, config->buffer_size, (gfsk_modem **) &result->rx_modem);
    if (code != 0) {
      return -1;
    }
    result->rx_modem_demodulate = gfsk_modem_demodulate;
    result->rx_modem_destroy = gfsk_modem_destroy;
  }

  return 0;
}

static int cli_create_tx_modem(app_config *config, struct cli_t *result) {
  if (config->tx_modem == MODEM_TYPE_GFSK) {
    int code = gfsk_modem_create(config->rx_req.gfsk, config->buffer_size, (gfsk_modem **) &result->tx_modem);
    if (code != 0) {
      return -1;
    }
    result->tx_modem_demodulate = gfsk_modem_demodulate;
    result->tx_modem_destroy = gfsk_modem_destroy;
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

  code = cli_create_tx_sdr(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  code = cli_create_tx_modem(config, result);
  if (code != 0) {
    cli_destroy(result);
    return -1;
  }

  if (config->tx_sdr_type != SDR_TYPE_NONE) {
    result->input_file = fopen(config->input_file, "wb");
    if (result->input_file == NULL) {
      fprintf(stderr, "<3>unable to open file %s: %s\n", config->input_file, strerror(errno));
      return -1;
    }
  }

  *output = result;
  return 0;
}

int cli_process(int argc, char **argv, cli *cli) {
  while (!cli->do_exit) {
    //FIXME handle
  }
  //close files and gracefully terminate
  cli_destroy(cli);
  return 0;
}

void cli_stop(cli *cli) {
  if (cli == NULL) {
    return;
  }
  cli->do_exit = 1;
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
    cli->rx_device->destroy(cli->tx_device->plugin);
    free(cli->tx_device);
  }
  if (cli->output_file != NULL) {
    fclose(cli->output_file);
  }
  if (cli->input_file != NULL) {
    fclose(cli->input_file);
  }
  if (cli->rx_modem != NULL) {
    cli->rx_modem_destroy(cli->rx_modem);
  }
  if (cli->tx_modem != NULL) {
    cli->tx_modem_destroy(cli->tx_modem);
  }
  free(cli);
}
