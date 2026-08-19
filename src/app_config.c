#include <libconfig.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"

#include <getopt.h>

char *read_and_copy_str(const config_setting_t *setting, const char *default_value) {
  const char *value;
  if (setting == NULL) {
    value = default_value;
  } else {
    value = config_setting_get_string(setting);
  }
  size_t length = strlen(value);
  char *result = malloc(sizeof(char) * length + 1);
  if (result == NULL) {
    return NULL;
  }
  strncpy(result, value, length);
  result[length] = '\0';
  return result;
}

static int app_config_convert_sdr_type(const char *type) {
  if (strcmp(type, "sdr-server") == 0) {
    return SDR_TYPE_SDR_SERVER;
  } else if (strcmp(type, "plutosdr") == 0) {
    return SDR_TYPE_PLUTOSDR;
  } else if (strcmp(type, "file") == 0) {
    return SDR_TYPE_FILE;
  } else if (strcmp(type, "none") == 0) {
    return SDR_TYPE_NONE;
  }
  return -1;
}

static int app_config_load_from_file(config_t *libconfig, const char *path, app_config *result) {
  fprintf(stdout, "loading configuration from: %s\n", path);

  int code = config_read_file(libconfig, path);
  if (code == CONFIG_FALSE) {
    fprintf(stderr, "<3>unable to read configuration: %s\n", config_error_text(libconfig));
    return -1;
  }

  const config_setting_t *setting = config_lookup(libconfig, "buffer_size");
  if (setting != NULL) {
    result->buffer_size = (uint32_t) config_setting_get_int(setting);
  }

  setting = config_lookup(libconfig, "bind_address");
  if (setting != NULL) {
    char *bind_address = read_and_copy_str(setting, "127.0.0.1");
    result->bind_address = bind_address;
  }
  setting = config_lookup(libconfig, "port");
  if (setting != NULL) {
    result->port = (uint16_t) config_setting_get_int(setting);
  }

  setting = config_lookup(libconfig, "read_timeout_seconds");
  if (setting != NULL) {
    result->read_timeout_seconds = config_setting_get_int(setting);
  }

  setting = config_lookup(libconfig, "queue_size");
  if (setting != NULL) {
    result->queue_size = config_setting_get_int(setting);
  }

  setting = config_lookup(libconfig, "rx_sdr_type");
  if (setting != NULL) {
    result->rx_sdr_type = app_config_convert_sdr_type(config_setting_get_string(setting));
  }

  if (result->rx_sdr_type == SDR_TYPE_SDR_SERVER) {
    setting = config_lookup(libconfig, "rx_sdr_server_address");
    if (setting != NULL) {
      result->rx_sdr_server_address = strdup(config_setting_get_string(setting));
      if (result->rx_sdr_server_address == NULL) {
        return -ENOMEM;
      }
    }
    setting = config_lookup(libconfig, "rx_sdr_server_port");
    if (setting != NULL) {
      result->rx_sdr_server_port = config_setting_get_int(setting);
    }
  }
  if (result->rx_sdr_type == SDR_TYPE_PLUTOSDR) {
    setting = config_lookup(libconfig, "rx_plutosdr_gain");
    if (setting != NULL) {
      result->rx_plutosdr_gain = config_setting_get_float(setting);
    }
  }

  setting = config_lookup(libconfig, "tx_sdr_type");
  if (setting != NULL) {
    result->tx_sdr_type = app_config_convert_sdr_type(config_setting_get_string(setting));
  }
  if (result->tx_sdr_type == SDR_TYPE_PLUTOSDR) {
    setting = config_lookup(libconfig, "tx_plutosdr_gain");
    if (setting != NULL) {
      result->tx_plutosdr_gain = config_setting_get_float(setting);
    }
    setting = config_lookup(libconfig, "tx_plutosdr_timeout_millis");
    if (setting != NULL) {
      result->tx_plutosdr_timeout_millis = config_setting_get_int(setting);
    }
  }

  return 0;
}

static int app_config_load_from_cli(int argc, char **argv, app_config *result) {
  enum {
    OPT_BIND_ADDRESS = 1000,
    OPT_PORT,
    OPT_BUFFER_SIZE,
    OPT_READ_TIMEOUT_SECONDS,
    OPT_QUEUE_SIZE,
    OPT_RX_SDR_TYPE,
    OPT_RX_SDR_SERVER_ADDRESS,
    OPT_RX_SDR_SERVER_PORT,
    OPT_TX_SDR_TYPE,
    OPT_TX_PLUTOSDR_GAIN,
    OPT_RX_PLUTOSDR_GAIN,
    OPT_TX_PLUTOSDR_TIMEOUT_MILLIS,
    OPT_TX_FILE,
    OPT_RX_FILE,
    OPT_CONFIG
  };
  static struct option long_options[] = {
    {"bind_address", required_argument, NULL, OPT_BIND_ADDRESS},
    {"port", required_argument, NULL, OPT_PORT},
    {"buffer_size", required_argument, NULL, OPT_BUFFER_SIZE},
    {"read_timeout_seconds", required_argument, NULL, OPT_READ_TIMEOUT_SECONDS},
    {"queue_size", required_argument, NULL, OPT_QUEUE_SIZE},
    {"rx_sdr_type", required_argument, NULL, OPT_RX_SDR_TYPE},
    {"rx_sdr_server_address", required_argument, NULL, OPT_RX_SDR_SERVER_ADDRESS},
    {"rx_sdr_server_port", required_argument, NULL, OPT_RX_SDR_SERVER_PORT},
    {"tx_sdr_type", required_argument, NULL, OPT_TX_SDR_TYPE},
    {"tx_plutosdr_gain", required_argument, NULL, OPT_TX_PLUTOSDR_GAIN},
    {"rx_plutosdr_gain", required_argument, NULL, OPT_RX_PLUTOSDR_GAIN},
    {"tx_plutosdr_timeout_millis", required_argument, NULL, OPT_TX_PLUTOSDR_TIMEOUT_MILLIS},
    {"tx_file", required_argument, NULL, OPT_TX_FILE},
    {"rx_file", required_argument, NULL, OPT_RX_FILE},
    {"config", required_argument, NULL, OPT_CONFIG},
    {NULL, 0, NULL, 0}
  };

  optind = 1;
  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (opt) {
      case OPT_BIND_ADDRESS: {
        char *bind_address = strdup(optarg);
        if (bind_address == NULL) {
          return -ENOMEM;
        }
        if (result->bind_address != NULL) {
          free(result->bind_address);
        }
        result->bind_address = bind_address;
        break;
      }
      case OPT_PORT:
        result->port = (uint16_t) atoi(optarg);
        break;
      case OPT_BUFFER_SIZE:
        result->buffer_size = (uint32_t) atoi(optarg);
        break;
      case OPT_READ_TIMEOUT_SECONDS: {
        result->read_timeout_seconds = atoi(optarg);
        break;
      }
      case OPT_QUEUE_SIZE:
        result->queue_size = (uint16_t) atoi(optarg);
        break;
      case OPT_RX_SDR_TYPE:
        result->rx_sdr_type = app_config_convert_sdr_type(optarg);
        break;
      case OPT_RX_SDR_SERVER_ADDRESS: {
        char *rx_sdr_server_address = strdup(optarg);
        if (rx_sdr_server_address == NULL) {
          return -ENOMEM;
        }
        if (result->rx_sdr_server_address != NULL) {
          free(result->rx_sdr_server_address);
        }
        result->rx_sdr_server_address = rx_sdr_server_address;
        break;
      }
      case OPT_RX_SDR_SERVER_PORT:
        result->rx_sdr_server_port = atoi(optarg);
        break;
      case OPT_TX_SDR_TYPE:
        result->tx_sdr_type = app_config_convert_sdr_type(optarg);
        break;
      case OPT_TX_PLUTOSDR_GAIN:
        result->tx_plutosdr_gain = atof(optarg);
        break;
      case OPT_RX_PLUTOSDR_GAIN:
        result->rx_plutosdr_gain = atof(optarg);
        break;
      case OPT_TX_PLUTOSDR_TIMEOUT_MILLIS:
        result->tx_plutosdr_timeout_millis = (unsigned int) atoi(optarg);
        break;
      case OPT_TX_FILE:
        result->tx_file = strdup(optarg);
        break;
      case OPT_RX_FILE:
        result->rx_file = strdup(optarg);
        break;
      case OPT_CONFIG:
      default:
        // already handled by app_config_create / unknown option
        break;
    }
  }
  return 0;
}

static int app_config_validate_and_log(app_config *result) {
  if (result->buffer_size == 0) {
    result->buffer_size = 262144;
  }
  fprintf(stdout, "buffer size: %d\n", result->buffer_size);
  if (result->port == 0) {
    result->port = 8091;
  }
  if (result->bind_address != NULL) {
    fprintf(stdout, "start listening on %s:%d\n", result->bind_address, result->port);
  }
  if (result->read_timeout_seconds < 0) {
    fprintf(stderr, "<3>read timeout should be positive: %d\n", result->read_timeout_seconds);
    return -1;
  }
  if (result->read_timeout_seconds == 0) {
    result->read_timeout_seconds = 5;
  }
  fprintf(stdout, "read timeout %ds\n", result->read_timeout_seconds);
  if (result->queue_size == 0) {
    result->queue_size = 64;
  }
  fprintf(stdout, "queue_size: %d\n", result->queue_size);
  if (result->rx_sdr_type < 0) {
    fprintf(stderr, "<3>invalid rx_sdr_type\n");
    return -1;
  }
  if (result->rx_sdr_type == 0) {
    result->rx_sdr_type = SDR_TYPE_SDR_SERVER;
  }
  if (result->rx_sdr_type == SDR_TYPE_SDR_SERVER) {
    if (result->rx_sdr_server_address == NULL) {
      result->rx_sdr_server_address = strdup("127.0.0.1");
    }
    if (result->rx_sdr_server_port == 0) {
      result->rx_sdr_server_port = 8090;
    }
    fprintf(stdout, "rx: sdr_server\n");
    fprintf(stdout, "sdr_server connection: %s:%d\n", result->rx_sdr_server_address, result->rx_sdr_server_port);
  }
  if (result->rx_sdr_type == SDR_TYPE_PLUTOSDR) {
    fprintf(stdout, "rx: plutosdr\n");
    fprintf(stdout, "rx_plutosdr_gain: %f\n", result->rx_plutosdr_gain);
  }
  if (result->rx_sdr_type == SDR_TYPE_FILE) {
    fprintf(stdout, "rx: file");
    if (result->bind_address != NULL) {
      fprintf(stderr, "<3>rx_sdr_type=file is not supported in the server mode\n");
      return -1;
    }
    if (result->rx_file == NULL) {
      fprintf(stderr, "<3>rx_file parameter is missing\n");
      return -1;
    }
    fprintf(stdout, "rx_file: %s\n", result->rx_file);
  }

  if (result->tx_sdr_type < 0) {
    fprintf(stderr, "<3>invalid tx_sdr_type\n");
    return -1;
  }
  if (result->tx_sdr_type == 0) {
    result->tx_sdr_type = SDR_TYPE_NONE;
  }
  if (result->tx_sdr_type == SDR_TYPE_PLUTOSDR) {
    fprintf(stdout, "tx: plutosdr\n");
    fprintf(stdout, "tx_plutosdr_gain: %f\n", result->tx_plutosdr_gain);
    if (result->tx_plutosdr_timeout_millis == 0) {
      result->tx_plutosdr_timeout_millis = 10000;
    }
    fprintf(stdout, "tx_plutosdr_timeout_millis: %d\n", result->tx_plutosdr_timeout_millis);
  }
  if (result->tx_sdr_type == SDR_TYPE_FILE) {
    fprintf(stdout, "tx: file\n");
    if (result->bind_address != NULL) {
      fprintf(stderr, "<3>tx_sdr_type=file is not supported in the server mode\n");
      return -1;
    }
    if (result->tx_file == NULL) {
      fprintf(stderr, "<3>tx_file parameter is missing\n");
      return -1;
    }
    fprintf(stdout, "tx_file: %s\n", result->tx_file);
  }
  if (result->tx_sdr_type == SDR_TYPE_NONE) {
    fprintf(stdout, "tx: none\n");
  }
  if (result->tx_sdr_type == SDR_TYPE_SDR_SERVER) {
    fprintf(stderr, "<3>sdr-server cannot tx. invalid tx_sdr_type parameter\n");
    return -1;
  }
  return 0;
}

int app_config_create(int argc, char **argv, app_config **config) {
  app_config *result = malloc(sizeof(app_config));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (app_config){0};

  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {NULL, 0, NULL, 0}
  };

  const char *config_path = NULL;
  optind = 1;
  int opt;
  while ((opt = getopt_long(argc, argv, "c:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'c':
        config_path = optarg;
        break;
    }
  }
  if (config_path != NULL) {
    config_t libconfig;
    config_init(&libconfig);
    int code = app_config_load_from_file(&libconfig, config_path, result);
    config_destroy(&libconfig);
    if (code != 0) {
      app_config_destroy(result);
      return code;
    }
  }

  int code = app_config_load_from_cli(argc, argv, result);
  if (code != 0) {
    app_config_destroy(result);
    return code;
  }

  code = app_config_validate_and_log(result);
  if (code != 0) {
    app_config_destroy(result);
    return code;
  }

  if (result->tx_sdr_type == SDR_TYPE_PLUTOSDR || result->rx_sdr_type == SDR_TYPE_PLUTOSDR) {
    code = iio_lib_create(&result->iio);
    if (code != 0) {
      app_config_destroy(result);
      return -1;
    }
  }

  *config = result;
  return 0;
}

void app_config_destroy(app_config *config) {
  if (config == NULL) {
    return;
  }
  if (config->bind_address != NULL) {
    free(config->bind_address);
  }
  if (config->rx_sdr_server_address != NULL) {
    free(config->rx_sdr_server_address);
  }
  if (config->iio != NULL) {
    iio_lib_destroy(config->iio);
  }
  if (config->rx_file != NULL) {
    free(config->rx_file);
  }
  if (config->tx_file != NULL) {
    free(config->tx_file);
  }
  free(config);
}
