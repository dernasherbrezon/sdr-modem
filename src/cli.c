#include "cli.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "sdr/file_source.h"
#include "sdr/plutosdr.h"
#include "sdr/sdr_device.h"
#include "sdr/sdr_server_client.h"

struct cli_t {
  sdr_device *rx_device;
  FILE *output_file;
  sdr_device *tx_device;
  FILE *input_file;
  volatile sig_atomic_t do_exit;
};

int cli_create(app_config *config, cli **output) {
  struct cli_t *result = malloc(sizeof(struct cli_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct cli_t){0};
  result->do_exit = 0;
  if (config->rx_sdr_type == SDR_TYPE_SDR_SERVER) {
    // FIXME
    struct sdr_rx rx = {
      .rx_center_freq = 0,
      .rx_offset = 0,
      .rx_sampling_freq = 0
    };
    int code = sdr_server_client_create(1, &rx, config->rx_sdr_server_address, config->rx_sdr_server_port, config->read_timeout_seconds, config->buffer_size, &result->rx_device);
    if (code != 0) {
      cli_destroy(result);
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_PLUTOSDR) {
    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
    if (rx_config == NULL) {
      cli_destroy(result);
      return -1;
    }
    // FIXME
    // rx_config->sample_rate = rx->rx_sampling_freq;
    // rx_config->center_freq = rx->rx_center_freq + rx->rx_offset;
    // rx_config->offset = rx->rx_offset;
    rx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    rx_config->manual_gain = config->rx_plutosdr_gain;
    int code = plutosdr_create(1, config->tx_sdr_type == SDR_TYPE_PLUTOSDR, rx_config, NULL, config->tx_plutosdr_timeout_millis, config->buffer_size, config->iio, &result->rx_device);
    if (code != 0) {
      cli_destroy(result);
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_FILE) {
    // FIXME
    int code = file_source_create(1, config->rx_file, NULL, 0, 0, config->buffer_size, &result->rx_device);
    if (code != 0) {
      cli_destroy(result);
      return -1;
    }
  } else if (config->rx_sdr_type == SDR_TYPE_NONE) {
    //do nothing, but supported
  } else {
    fprintf(stderr, "<3>unsupported rx_sdr_type: %d\n", config->rx_sdr_type);
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

  if (config->tx_sdr_type == SDR_TYPE_PLUTOSDR) {
    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
    if (tx_config == NULL) {
      cli_destroy(result);
      return -1;
    }
    //FIXME
    // tx_config->sample_rate = req->gfsk->sample_rate;
    // tx_config->center_freq = req->gfsk->center_freq;
    tx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
    tx_config->manual_gain = config->tx_plutosdr_gain;
    int code = plutosdr_create(1, false, NULL, tx_config, config->tx_plutosdr_timeout_millis, config->buffer_size, config->iio, &result->tx_device);
    if (code != 0) {
      cli_destroy(result);
      return -1;
    }
  } else if (config->tx_sdr_type == SDR_TYPE_FILE) {
    // FIXME
    int code = file_source_create(1, config->tx_file, NULL, 0, 0, config->buffer_size, &result->tx_device);
    if (code != 0) {
      cli_destroy(result);
      return -1;
    }
  } else if (config->tx_sdr_type == SDR_TYPE_NONE) {
    //do nothing, but supported
  } else {
    fprintf(stderr, "<3>unsupported rx_sdr_type: %d\n", config->rx_sdr_type);
    return -1;
  }

  if (config->tx_sdr_type != SDR_TYPE_NONE) {
    result->input_file = fopen(config->input_file, "wb");
    if (result->input_file == NULL) {
      fprintf(stderr, "<3>unable to open file %s: %s\n", config->input_file, strerror(errno));
      cli_destroy(result);
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
  free(cli);
}
