#include <libconfig.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp/sdr_modem.h"
#include "app_config.h"

#include <getopt.h>

static sdr_device_type app_config_convert_sdr_type(const char *type) {
  if (strcmp(type, "sdr-server") == 0) {
    return SDR_TYPE_SDR_SERVER;
  } else if (strcmp(type, "plutosdr") == 0) {
    return SDR_TYPE_PLUTOSDR;
  } else if (strcmp(type, "file") == 0) {
    return SDR_TYPE_FILE;
  }
  return SDR_TYPE_INVALID;
}

static int app_config_parse_gain_mode(const char *str, iio_gain_mode *result) {
  if (strcmp(str, "manual") == 0) {
    *result = IIO_GAIN_MODE_MANUAL;
  } else if (strcmp(str, "fast_attack") == 0) {
    *result = IIO_GAIN_MODE_FAST_ATTACK;
  } else if (strcmp(str, "slow_attack") == 0) {
    *result = IIO_GAIN_MODE_SLOW_ATTACK;
  } else if (strcmp(str, "hybrid") == 0) {
    *result = IIO_GAIN_MODE_HYBRID;
  } else {
    fprintf(stderr, "<3>invalid plutosdr_gain_control_mode: %s. expected manual, fast_attack, slow_attack or hybrid\n", str);
    return -1;
  }
  return 0;
}

static sdr_file_format app_config_convert_file_format(const char *format) {
  if (strcmp(format, "cu8") == 0) {
    return FILE_FORMAT_CU8;
  } else if (strcmp(format, "cf32") == 0) {
    return FILE_FORMAT_CF32;
  } else if (strcmp(format, "cs16") == 0) {
    return FILE_FORMAT_CS16;
  }
  return FILE_FORMAT_INVALID;
}

static sdr_file_format app_config_guess_file_format(const char *filename) {
  if (filename == NULL) {
    return FILE_FORMAT_INVALID;
  }
  size_t len = strlen(filename);
  if (len > 3 && strcmp(filename + len - 3, ".gz") == 0) {
    len -= 3;
  }
  if (len > 5 && strncmp(filename + len - 5, ".cf32", 5) == 0) {
    return FILE_FORMAT_CF32;
  }
  if (len > 4 && strncmp(filename + len - 4, ".cu8", 4) == 0) {
    return FILE_FORMAT_CU8;
  }
  if (len > 5 && strncmp(filename + len - 5, ".cs16", 5) == 0) {
    return FILE_FORMAT_CS16;
  }
  return FILE_FORMAT_INVALID;
}

static sdr_modem_type app_config_convert_modem_type(const char *type) {
  if (strcmp(type, "gfsk") == 0) {
    return MODEM_TYPE_GFSK;
  } else if (strcmp(type, "bpsk") == 0) {
    return MODEM_TYPE_BPSK;
  } else if (strcmp(type, "dpsk") == 0) {
    return MODEM_TYPE_DPSK;
  } else if (strcmp(type, "sdpsk") == 0) {
    return MODEM_TYPE_SDPSK;
  } else if (strcmp(type, "psk_pm") == 0) {
    return MODEM_TYPE_PSK_PM;
  }
  return MODEM_TYPE_INVALID;
}

static int app_config_convert_framing_type(const char *type) {
  if (strcmp(type, "none") == 0) {
    return FRAMING_TYPE_NONE;
  }
  return -1;
}

static int app_config_hex_to_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

// syncword is a hex string, msb first, e.g. "CCCCCCFE". optional "0x" prefix.
// every hex digit is 4 bits, so syncword_bits is calculated from the string length
static int app_config_parse_syncword(const char *hex, gfsk_modem_settings *settings) {
  if (strncmp(hex, "0x", 2) == 0 || strncmp(hex, "0X", 2) == 0) {
    hex += 2;
  }
  size_t hex_len = strlen(hex);
  if (hex_len == 0 || hex_len > sizeof(uint64_t) * 2) {
    fprintf(stderr, "<3>invalid syncword. expected 1-%zu hex digits: %s\n", sizeof(uint64_t) * 2, hex);
    return -EINVAL;
  }
  uint64_t syncword = 0;
  for (size_t i = 0; i < hex_len; i++) {
    int nibble = app_config_hex_to_nibble(hex[i]);
    if (nibble < 0) {
      fprintf(stderr, "<3>invalid syncword. expected hex digits: %s\n", hex);
      return -EINVAL;
    }
    syncword = (syncword << 4) | (uint64_t) nibble;
  }
  settings->syncword = syncword;
  settings->syncword_bits = (uint32_t) (hex_len * 4);
  return 0;
}

static int app_config_convert_direction(const char *direction) {
  if (strcmp(direction, "rx") == 0) {
    return DIRECTION_RX;
  } else if (strcmp(direction, "tx") == 0) {
    return DIRECTION_TX;
  }
  return -1;
}

// overwrites the string only if new value is present
static int app_config_replace_str(const char *value, char **to) {
  char *copy = strdup(value);
  if (copy == NULL) {
    return -ENOMEM;
  }
  if (*to != NULL) {
    free(*to);
  }
  *to = copy;
  return 0;
}

static void app_config_load_gfsk_from_file(config_t *libconfig, app_config *result) {
  gfsk_modem_settings *settings = &result->gfsk;
  const config_setting_t *setting;

  setting = config_lookup(libconfig, "gfsk_baud_rate");
  if (setting != NULL) {
    settings->baud_rate = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "gfsk_deviation");
  if (setting != NULL) {
    settings->deviation = config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "gfsk_bandwidth");
  if (setting != NULL) {
    settings->bandwidth = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "gfsk_bt");
  if (setting != NULL) {
    settings->bt = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "gfsk_use_dc_block");
  if (setting != NULL) {
    settings->use_dc_block = config_setting_get_bool(setting) ? true : false;
  }
}

// bpsk, dpsk and sdpsk all share the same settings shape (bpsk_modem_settings), so config keys and
// cli flags for all three are named with a common "psk" prefix rather than being duplicated per type
static void app_config_load_psk_from_file(config_t *libconfig, app_config *result) {
  bpsk_modem_settings *settings = &result->psk;
  const config_setting_t *setting;

  setting = config_lookup(libconfig, "psk_baud_rate");
  if (setting != NULL) {
    settings->baud_rate = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_rrc_beta");
  if (setting != NULL) {
    settings->rrc_beta = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_rrc_delay");
  if (setting != NULL) {
    settings->rrc_delay = (unsigned int) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_costas_bandwidth");
  if (setting != NULL) {
    settings->costas_bandwidth = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_symsync_filter_bank_size");
  if (setting != NULL) {
    settings->symsync_filter_bank_size = (unsigned int) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_bandwidth");
  if (setting != NULL) {
    settings->bandwidth = (uint32_t) config_setting_get_int(setting);
  }
}

static void app_config_load_psk_pm_from_file(config_t *libconfig, app_config *result) {
  psk_pm_modem_settings *settings = &result->psk_pm;
  const config_setting_t *setting;

  setting = config_lookup(libconfig, "psk_pm_baud_rate");
  if (setting != NULL) {
    settings->baud_rate = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_rrc_beta");
  if (setting != NULL) {
    settings->rrc_beta = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_rrc_delay");
  if (setting != NULL) {
    settings->rrc_delay = (unsigned int) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_costas_bandwidth");
  if (setting != NULL) {
    settings->costas_bandwidth = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_symsync_filter_bank_size");
  if (setting != NULL) {
    settings->symsync_filter_bank_size = (unsigned int) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_subcarrier_frequency");
  if (setting != NULL) {
    settings->subcarrier_frequency = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_modulation_index");
  if (setting != NULL) {
    settings->modulation_index = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_carrier_pll_bandwidth");
  if (setting != NULL) {
    settings->carrier_pll_bandwidth = (float) config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_subcarrier_bandwidth");
  if (setting != NULL) {
    settings->subcarrier_bandwidth = (uint32_t) config_setting_get_int(setting);
  }
}

// frequency and sample_rate are not loaded here: they are shared with other sdr types and modems,
// so they are taken from the generic "frequency" and "sample_rate" settings at the time of use
static int app_config_load_sdr_server_from_file(config_t *libconfig, app_config *result) {
  sdr_server_settings *settings = &result->sdr_server;
  const config_setting_t *setting;

  setting = config_lookup(libconfig, "sdr_server_address");
  if (setting != NULL) {
    int code = app_config_replace_str(config_setting_get_string(setting), &settings->addr);
    if (code != 0) {
      return code;
    }
  }
  setting = config_lookup(libconfig, "sdr_server_port");
  if (setting != NULL) {
    settings->port = config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "sdr_server_read_timeout_seconds");
  if (setting != NULL) {
    settings->read_timeout_seconds = config_setting_get_int(setting);
  }
  return 0;
}

// frequency and sample_rate are not loaded here: they are shared with other sdr types and modems,
// so they are taken from the generic "frequency" and "sample_rate" settings at the time of use
static int app_config_load_sdr_file_from_file(config_t *libconfig, app_config *result) {
  sdr_file_settings *settings = &result->sdr_file;
  const config_setting_t *setting;

  setting = config_lookup(libconfig, "rx_file");
  if (setting != NULL) {
    int code = app_config_replace_str(config_setting_get_string(setting), &settings->rx_file);
    if (code != 0) {
      return code;
    }
  }
  setting = config_lookup(libconfig, "rx_file_format");
  if (setting != NULL) {
    settings->rx_file_format = app_config_convert_file_format(config_setting_get_string(setting));
  }
  setting = config_lookup(libconfig, "tx_file");
  if (setting != NULL) {
    int code = app_config_replace_str(config_setting_get_string(setting), &settings->tx_file);
    if (code != 0) {
      return code;
    }
  }
  setting = config_lookup(libconfig, "tx_file_format");
  if (setting != NULL) {
    settings->tx_file_format = app_config_convert_file_format(config_setting_get_string(setting));
  }
  return 0;
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
    code = app_config_replace_str(config_setting_get_string(setting), &result->bind_address);
    if (code != 0) {
      return code;
    }
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

  setting = config_lookup(libconfig, "direction");
  if (setting != NULL) {
    result->direction = app_config_convert_direction(config_setting_get_string(setting));
  }

  setting = config_lookup(libconfig, "sdr_type");
  if (setting != NULL) {
    result->sdr_type = app_config_convert_sdr_type(config_setting_get_string(setting));
  }

  code = app_config_load_sdr_server_from_file(libconfig, result);
  if (code != 0) {
    return code;
  }
  if (result->sdr_type == SDR_TYPE_PLUTOSDR) {
    setting = config_lookup(libconfig, "plutosdr_gain_control_mode");
    if (setting != NULL) {
      code = app_config_parse_gain_mode(config_setting_get_string(setting), &result->plutosdr.gain_control_mode);
      if (code != 0) {
        return code;
      }
    }
    setting = config_lookup(libconfig, "plutosdr_tx_powerdown");
    if (setting != NULL) {
      result->plutosdr.tx_powerdown = config_setting_get_bool(setting) ? true : false;
    }
    setting = config_lookup(libconfig, "plutosdr_timeout_ms");
    if (setting != NULL) {
      result->plutosdr.timeout_ms = config_setting_get_int(setting);
    }
  }
  code = app_config_load_sdr_file_from_file(libconfig, result);
  if (code != 0) {
    return code;
  }

  setting = config_lookup(libconfig, "modem");
  if (setting != NULL) {
    result->modem = app_config_convert_modem_type(config_setting_get_string(setting));
  }
  setting = config_lookup(libconfig, "frequency");
  if (setting != NULL) {
    result->frequency = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "sample_rate");
  if (setting != NULL) {
    result->sample_rate = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "gain");
  if (setting != NULL) {
    result->gain = config_setting_get_float(setting);
  }
  app_config_load_gfsk_from_file(libconfig, result);
  app_config_load_psk_from_file(libconfig, result);
  app_config_load_psk_pm_from_file(libconfig, result);
  setting = config_lookup(libconfig, "framing");
  if (setting != NULL) {
    result->framing = app_config_convert_framing_type(config_setting_get_string(setting));
    //ignore framing for now
  }
  setting = config_lookup(libconfig, "syncword");
  if (setting != NULL) {
    code = app_config_parse_syncword(config_setting_get_string(setting), &result->gfsk);
    if (code != 0) {
      return code;
    }
  }

  setting = config_lookup(libconfig, "debug_constellation_file");
  if (setting != NULL) {
    code = app_config_replace_str(config_setting_get_string(setting), &result->debug_constellation_file);
    if (code != 0) {
      return code;
    }
  }

  setting = config_lookup(libconfig, "debug_baseband_file");
  if (setting != NULL) {
    code = app_config_replace_str(config_setting_get_string(setting), &result->debug_baseband_file);
    if (code != 0) {
      return code;
    }
  }

  setting = config_lookup(libconfig, "debug_subcarrier_file");
  if (setting != NULL) {
    code = app_config_replace_str(config_setting_get_string(setting), &result->debug_subcarrier_file);
    if (code != 0) {
      return code;
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
    OPT_DIRECTION,
    OPT_SDR_TYPE,
    OPT_SDR_SERVER_ADDRESS,
    OPT_SDR_SERVER_PORT,
    OPT_SDR_SERVER_READ_TIMEOUT_SECONDS,
    OPT_PLUTOSDR_GAIN_CONTROL_MODE,
    OPT_PLUTOSDR_TX_POWERDOWN,
    OPT_PLUTOSDR_TIMEOUT_MS,
    OPT_RX_FILE,
    OPT_RX_FILE_FORMAT,
    OPT_TX_FILE,
    OPT_TX_FILE_FORMAT,
    OPT_CONFIG,
    OPT_INPUT,
    OPT_OUTPUT,
    OPT_MODEM,
    OPT_FRAMING,
    OPT_SYNCWORD,
    OPT_FREQUENCY,
    OPT_SAMPLE_RATE,
    OPT_GFSK_BAUD_RATE,
    OPT_GFSK_DEVIATION,
    OPT_GFSK_BANDWIDTH,
    OPT_GFSK_BT,
    OPT_GFSK_USE_DC_BLOCK,
    OPT_PSK_BAUD_RATE,
    OPT_PSK_RRC_BETA,
    OPT_PSK_RRC_DELAY,
    OPT_PSK_COSTAS_BANDWIDTH,
    OPT_PSK_SYMSYNC_FILTER_BANK_SIZE,
    OPT_PSK_BANDWIDTH,
    OPT_PSK_PM_BAUD_RATE,
    OPT_PSK_PM_RRC_BETA,
    OPT_PSK_PM_RRC_DELAY,
    OPT_PSK_PM_COSTAS_BANDWIDTH,
    OPT_PSK_PM_SYMSYNC_FILTER_BANK_SIZE,
    OPT_PSK_PM_SUBCARRIER_FREQUENCY,
    OPT_PSK_PM_MODULATION_INDEX,
    OPT_PSK_PM_CARRIER_PLL_BANDWIDTH,
    OPT_PSK_PM_SUBCARRIER_BANDWIDTH,
    OPT_FREQ_OFFSET_FILE,
    OPT_DEBUG_FREQ_OFFSET_FILE,
    OPT_DEBUG_CONSTELLATION_FILE,
    OPT_DEBUG_BASEBAND_FILE,
    OPT_DEBUG_SUBCARRIER_FILE
  };
  static struct option long_options[] = {
    {"bind_address", required_argument, NULL, OPT_BIND_ADDRESS},
    {"port", required_argument, NULL, OPT_PORT},
    {"buffer_size", required_argument, NULL, OPT_BUFFER_SIZE},
    {"read_timeout_seconds", required_argument, NULL, OPT_READ_TIMEOUT_SECONDS},
    {"queue_size", required_argument, NULL, OPT_QUEUE_SIZE},
    {"direction", required_argument, NULL, OPT_DIRECTION},
    {"sdr_type", required_argument, NULL, OPT_SDR_TYPE},
    {"sdr_server_address", required_argument, NULL, OPT_SDR_SERVER_ADDRESS},
    {"sdr_server_port", required_argument, NULL, OPT_SDR_SERVER_PORT},
    {"sdr_server_read_timeout_seconds", required_argument, NULL, OPT_SDR_SERVER_READ_TIMEOUT_SECONDS},
    {"plutosdr_gain_control_mode", required_argument, NULL, OPT_PLUTOSDR_GAIN_CONTROL_MODE},
    {"plutosdr_tx_powerdown", required_argument, NULL, OPT_PLUTOSDR_TX_POWERDOWN},
    {"plutosdr_timeout_ms", required_argument, NULL, OPT_PLUTOSDR_TIMEOUT_MS},
    {"rx_file", required_argument, NULL, OPT_RX_FILE},
    {"rx_file_format", required_argument, NULL, OPT_RX_FILE_FORMAT},
    {"tx_file", required_argument, NULL, OPT_TX_FILE},
    {"tx_file_format", required_argument, NULL, OPT_TX_FILE_FORMAT},
    {"config", required_argument, NULL, OPT_CONFIG},
    {"input", required_argument, NULL, OPT_INPUT},
    {"output", required_argument, NULL, OPT_OUTPUT},
    {"modem", required_argument, NULL, OPT_MODEM},
    {"framing", required_argument, NULL, OPT_FRAMING},
    {"syncword", required_argument, NULL, OPT_SYNCWORD},
    {"frequency", required_argument, NULL, OPT_FREQUENCY},
    {"sample_rate", required_argument, NULL, OPT_SAMPLE_RATE},
    {"gfsk_baud_rate", required_argument, NULL, OPT_GFSK_BAUD_RATE},
    {"gfsk_deviation", required_argument, NULL, OPT_GFSK_DEVIATION},
    {"gfsk_bandwidth", required_argument, NULL, OPT_GFSK_BANDWIDTH},
    {"gfsk_bt", required_argument, NULL, OPT_GFSK_BT},
    {"gfsk_use_dc_block", required_argument, NULL, OPT_GFSK_USE_DC_BLOCK},
    {"psk_baud_rate", required_argument, NULL, OPT_PSK_BAUD_RATE},
    {"psk_rrc_beta", required_argument, NULL, OPT_PSK_RRC_BETA},
    {"psk_rrc_delay", required_argument, NULL, OPT_PSK_RRC_DELAY},
    {"psk_costas_bandwidth", required_argument, NULL, OPT_PSK_COSTAS_BANDWIDTH},
    {"psk_symsync_filter_bank_size", required_argument, NULL, OPT_PSK_SYMSYNC_FILTER_BANK_SIZE},
    {"psk_bandwidth", required_argument, NULL, OPT_PSK_BANDWIDTH},
    {"psk_pm_baud_rate", required_argument, NULL, OPT_PSK_PM_BAUD_RATE},
    {"psk_pm_rrc_beta", required_argument, NULL, OPT_PSK_PM_RRC_BETA},
    {"psk_pm_rrc_delay", required_argument, NULL, OPT_PSK_PM_RRC_DELAY},
    {"psk_pm_costas_bandwidth", required_argument, NULL, OPT_PSK_PM_COSTAS_BANDWIDTH},
    {"psk_pm_symsync_filter_bank_size", required_argument, NULL, OPT_PSK_PM_SYMSYNC_FILTER_BANK_SIZE},
    {"psk_pm_subcarrier_frequency", required_argument, NULL, OPT_PSK_PM_SUBCARRIER_FREQUENCY},
    {"psk_pm_modulation_index", required_argument, NULL, OPT_PSK_PM_MODULATION_INDEX},
    {"psk_pm_carrier_pll_bandwidth", required_argument, NULL, OPT_PSK_PM_CARRIER_PLL_BANDWIDTH},
    {"psk_pm_subcarrier_bandwidth", required_argument, NULL, OPT_PSK_PM_SUBCARRIER_BANDWIDTH},
    {"freq_offset_file", required_argument, NULL, OPT_FREQ_OFFSET_FILE},
    {"debug_freq_offset_file", required_argument, NULL, OPT_DEBUG_FREQ_OFFSET_FILE},
    {"debug_constellation_file", required_argument, NULL, OPT_DEBUG_CONSTELLATION_FILE},
    {"debug_baseband_file", required_argument, NULL, OPT_DEBUG_BASEBAND_FILE},
    {"debug_subcarrier_file", required_argument, NULL, OPT_DEBUG_SUBCARRIER_FILE},
    {NULL, 0, NULL, 0}
  };

  optind = 1;
  opterr = 1;
  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (opt) {
      case OPT_BIND_ADDRESS: {
        int code = app_config_replace_str(optarg, &result->bind_address);
        if (code != 0) {
          return code;
        }
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
      case OPT_DIRECTION:
        result->direction = app_config_convert_direction(optarg);
        break;
      case OPT_SDR_TYPE:
        result->sdr_type = app_config_convert_sdr_type(optarg);
        break;
      case OPT_SDR_SERVER_ADDRESS: {
        int code = app_config_replace_str(optarg, &result->sdr_server.addr);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_SDR_SERVER_PORT:
        result->sdr_server.port = atoi(optarg);
        break;
      case OPT_SDR_SERVER_READ_TIMEOUT_SECONDS:
        result->sdr_server.read_timeout_seconds = atoi(optarg);
        break;
      case OPT_PLUTOSDR_GAIN_CONTROL_MODE: {
        int code = app_config_parse_gain_mode(optarg, &result->plutosdr.gain_control_mode);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_PLUTOSDR_TX_POWERDOWN:
        result->plutosdr.tx_powerdown = (strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0);
        break;
      case OPT_PLUTOSDR_TIMEOUT_MS:
        result->plutosdr.timeout_ms = (unsigned int) atoi(optarg);
        break;
      case OPT_RX_FILE: {
        int code = app_config_replace_str(optarg, &result->sdr_file.rx_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_RX_FILE_FORMAT:
        result->sdr_file.rx_file_format = app_config_convert_file_format(optarg);
        break;
      case OPT_TX_FILE: {
        int code = app_config_replace_str(optarg, &result->sdr_file.tx_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_TX_FILE_FORMAT:
        result->sdr_file.tx_file_format = app_config_convert_file_format(optarg);
        break;
      case OPT_INPUT: {
        int code = app_config_replace_str(optarg, &result->input_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_OUTPUT: {
        int code = app_config_replace_str(optarg, &result->output_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_MODEM:
        result->modem = app_config_convert_modem_type(optarg);
        break;
      case OPT_FRAMING:
        result->framing = app_config_convert_framing_type(optarg);
        break;
      case OPT_SYNCWORD: {
        int code = app_config_parse_syncword(optarg, &result->gfsk);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_FREQUENCY:
        result->frequency = strtoull(optarg, NULL, 10);
        break;
      case OPT_SAMPLE_RATE:
        result->sample_rate = strtoull(optarg, NULL, 10);
        break;
      case OPT_GFSK_BAUD_RATE:
        result->gfsk.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_GFSK_DEVIATION:
        result->gfsk.deviation = strtoll(optarg, NULL, 10);
        break;
      case OPT_GFSK_BANDWIDTH:
        result->gfsk.bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_GFSK_BT:
        result->gfsk.bt = (float) atof(optarg);
        break;
      case OPT_GFSK_USE_DC_BLOCK:
        result->gfsk.use_dc_block = (strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0);
        break;
      case OPT_PSK_BAUD_RATE:
        result->psk.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_RRC_BETA:
        result->psk.rrc_beta = (float) atof(optarg);
        break;
      case OPT_PSK_RRC_DELAY:
        result->psk.rrc_delay = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_COSTAS_BANDWIDTH:
        result->psk.costas_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_SYMSYNC_FILTER_BANK_SIZE:
        result->psk.symsync_filter_bank_size = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_BANDWIDTH:
        result->psk.bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_BAUD_RATE:
        result->psk_pm.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_RRC_BETA:
        result->psk_pm.rrc_beta = (float) atof(optarg);
        break;
      case OPT_PSK_PM_RRC_DELAY:
        result->psk_pm.rrc_delay = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_COSTAS_BANDWIDTH:
        result->psk_pm.costas_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_PM_SYMSYNC_FILTER_BANK_SIZE:
        result->psk_pm.symsync_filter_bank_size = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_SUBCARRIER_FREQUENCY:
        result->psk_pm.subcarrier_frequency = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_MODULATION_INDEX:
        result->psk_pm.modulation_index = (float) atof(optarg);
        break;
      case OPT_PSK_PM_CARRIER_PLL_BANDWIDTH:
        result->psk_pm.carrier_pll_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_PM_SUBCARRIER_BANDWIDTH:
        result->psk_pm.subcarrier_bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_FREQ_OFFSET_FILE: {
        int code = app_config_replace_str(optarg, &result->freq_offset_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_DEBUG_FREQ_OFFSET_FILE: {
        int code = app_config_replace_str(optarg, &result->debug_freq_offset_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_DEBUG_CONSTELLATION_FILE: {
        int code = app_config_replace_str(optarg, &result->debug_constellation_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_DEBUG_BASEBAND_FILE: {
        int code = app_config_replace_str(optarg, &result->debug_baseband_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_DEBUG_SUBCARRIER_FILE: {
        int code = app_config_replace_str(optarg, &result->debug_subcarrier_file);
        if (code != 0) {
          return code;
        }
        break;
      }
      case OPT_CONFIG:
      default:
        // already handled by app_config_create / unknown option
        break;
    }
  }

  return 0;
}

static void app_config_apply_psk_defaults(bpsk_modem_settings *settings) {
  if (settings->symsync_filter_bank_size == 0) {
    settings->symsync_filter_bank_size = 32;
  }
  if (settings->rrc_delay == 0) {
    settings->rrc_delay = 5;
  }
  if (settings->rrc_beta == 0.0f) {
    settings->rrc_beta = 0.35f;
  }
  if (settings->costas_bandwidth == 0.0f) {
    settings->costas_bandwidth = 0.01f;
  }
}

static void app_config_apply_psk_pm_defaults(psk_pm_modem_settings *settings) {
  if (settings->symsync_filter_bank_size == 0) {
    settings->symsync_filter_bank_size = 32;
  }
  if (settings->rrc_delay == 0) {
    settings->rrc_delay = 5;
  }
  if (settings->rrc_beta == 0.0f) {
    settings->rrc_beta = 0.35f;
  }
  if (settings->costas_bandwidth == 0.0f) {
    settings->costas_bandwidth = 0.005f;
  }
  if (settings->carrier_pll_bandwidth == 0.0f) {
    settings->carrier_pll_bandwidth = 0.0001f;
  }
}

// guesses the format from the file extension if not set explicitly. file is optional
static int app_config_validate_file(const char *name, const char *file, sdr_file_format *format) {
  if (file == NULL) {
    return 0;
  }
  fprintf(stdout, "%s: %s\n", name, file);
  if (*format == FILE_FORMAT_GUESS) {
    *format = app_config_guess_file_format(file);
  }
  if (*format == FILE_FORMAT_INVALID) {
    fprintf(stderr, "<3>invalid or unable to guess %s_format\n", name);
    return -1;
  }
  fprintf(stdout, "%s_format: %s\n", name, *format == FILE_FORMAT_CU8 ? "cu8" : (*format == FILE_FORMAT_CS16 ? "cs16" : "cf32"));
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

  bool is_cli_mode = (result->bind_address == NULL);
  if (is_cli_mode) {
    if (result->direction < 0) {
      fprintf(stderr, "<3>invalid direction\n");
      return -1;
    }
    if (result->direction == 0) {
      fprintf(stderr, "<3>direction is required in cli mode. use --direction rx|tx\n");
      return -1;
    }
    fprintf(stdout, "direction: %s\n", result->direction == DIRECTION_RX ? "rx" : "tx");
  }

  if (result->sdr_type == SDR_TYPE_SDR_SERVER) {
    if (is_cli_mode && result->direction == DIRECTION_TX) {
      fprintf(stderr, "<3>sdr-server cannot tx. invalid sdr_type parameter\n");
      return -1;
    }
    if (result->sdr_server.addr == NULL) {
      result->sdr_server.addr = strdup("127.0.0.1");
      if (result->sdr_server.addr == NULL) {
        return -ENOMEM;
      }
    }
    if (result->sdr_server.port == 0) {
      result->sdr_server.port = 8090;
    }
    if (result->sdr_server.read_timeout_seconds < 0) {
      fprintf(stderr, "<3>sdr_server read timeout should be positive: %d\n", result->sdr_server.read_timeout_seconds);
      return -1;
    }
    if (result->sdr_server.read_timeout_seconds == 0) {
      result->sdr_server.read_timeout_seconds = result->read_timeout_seconds;
    }
    fprintf(stdout, "sdr: sdr_server\n");
    fprintf(stdout, "sdr_server connection: %s:%d\n", result->sdr_server.addr, result->sdr_server.port);
    fprintf(stdout, "sdr_server read timeout %ds\n", result->sdr_server.read_timeout_seconds);
  } else if (result->sdr_type == SDR_TYPE_PLUTOSDR) {
    fprintf(stdout, "sdr: plutosdr\n");
    fprintf(stdout, "plutosdr_gain_control_mode: %d\n", result->plutosdr.gain_control_mode);
    fprintf(stdout, "plutosdr_tx_powerdown: %s\n", result->plutosdr.tx_powerdown ? "true" : "false");
    if (result->plutosdr.timeout_ms == 0) {
      result->plutosdr.timeout_ms = 10000;
    }
    fprintf(stdout, "plutosdr_timeout_ms: %d\n", result->plutosdr.timeout_ms);
  } else if (result->sdr_type == SDR_TYPE_FILE) {
    fprintf(stdout, "sdr: file\n");
    if (!is_cli_mode) {
      fprintf(stderr, "<3>sdr_type=file is not supported in the server mode\n");
      return -1;
    }
    if (result->direction == DIRECTION_RX && result->sdr_file.rx_file == NULL) {
      fprintf(stderr, "<3>rx_file parameter is missing\n");
      return -1;
    }
    if (result->direction == DIRECTION_TX && result->sdr_file.tx_file == NULL) {
      fprintf(stderr, "<3>tx_file parameter is missing\n");
      return -1;
    }
    if (app_config_validate_file("rx_file", result->sdr_file.rx_file, &result->sdr_file.rx_file_format) != 0) {
      return -1;
    }
    if (app_config_validate_file("tx_file", result->sdr_file.tx_file, &result->sdr_file.tx_file_format) != 0) {
      return -1;
    }
  } else {
    fprintf(stderr, "<3>invalid sdr_type: %d\n", result->sdr_type);
    return -1;
  }

  if (is_cli_mode && result->direction == DIRECTION_RX && result->output_file == NULL) {
    fprintf(stderr, "<3>rx is enabled, but the output file is missing\n");
    return -1;
  }
  if (is_cli_mode && result->direction == DIRECTION_TX && result->input_file == NULL) {
    fprintf(stderr, "<3>tx is enabled, but the input file is missing\n");
    return -1;
  }

  if (result->modem == MODEM_TYPE_INVALID) {
    fprintf(stderr, "<3>invalid modem\n");
    return -1;
  }
  switch (result->modem) {
    case MODEM_TYPE_BPSK:
      result->psk.type = BPSK;
      break;
    case MODEM_TYPE_DPSK:
      result->psk.type = DPSK;
      break;
    case MODEM_TYPE_SDPSK:
      result->psk.type = SDPSK;
      break;
    default:
      // do nothing
      break;
  }
  app_config_apply_psk_defaults(&result->psk);
  app_config_apply_psk_pm_defaults(&result->psk_pm);

  if (result->framing < 0) {
    fprintf(stderr, "<3>invalid framing\n");
    return -1;
  }
  if (result->gfsk.syncword_bits != 0) {
    fprintf(stdout, "syncword: 0x%0*llX bits: %u\n", (int) (result->gfsk.syncword_bits / 4), (unsigned long long) result->gfsk.syncword, result->gfsk.syncword_bits);
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
  opterr = 0; // ignore extra options that can appear
  int opt;
  // leading '+' disables GNU getopt's argv permutation: this pass only knows
  // about "-c/--config" and must not reorder/consume the other long flags,
  // since app_config_load_from_cli() does the real, full parse afterwards
  while ((opt = getopt_long(argc, argv, "+c:", long_options, NULL)) != -1) {
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

  if (result->sdr_type == SDR_TYPE_PLUTOSDR) {
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
  if (config->sdr_server.addr != NULL) {
    free(config->sdr_server.addr);
  }
  if (config->freq_offset_file != NULL) {
    free(config->freq_offset_file);
  }
  if (config->debug_freq_offset_file != NULL) {
    free(config->debug_freq_offset_file);
  }
  if (config->debug_constellation_file != NULL) {
    free(config->debug_constellation_file);
  }
  if (config->debug_baseband_file != NULL) {
    free(config->debug_baseband_file);
  }
  if (config->debug_subcarrier_file != NULL) {
    free(config->debug_subcarrier_file);
  }
  if (config->iio != NULL) {
    iio_lib_destroy(config->iio);
  }
  if (config->sdr_file.rx_file != NULL) {
    free(config->sdr_file.rx_file);
  }
  if (config->sdr_file.tx_file != NULL) {
    free(config->sdr_file.tx_file);
  }
  if (config->input_file != NULL) {
    free(config->input_file);
  }
  if (config->output_file != NULL) {
    free(config->output_file);
  }
  free(config);
}
