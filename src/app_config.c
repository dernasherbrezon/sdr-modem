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
  }
  return -1;
}

static int app_config_convert_file_format(const char *format) {
  if (strcmp(format, "cu8") == 0) {
    return FILE_FORMAT_CU8;
  } else if (strcmp(format, "cf32") == 0) {
    return FILE_FORMAT_CF32;
  } else if (strcmp(format, "cs16") == 0) {
    return FILE_FORMAT_CS16;
  }
  return -1;
}

static int app_config_guess_file_format(const char *filename) {
  if (filename == NULL) {
    return -1;
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
  return -1;
}

static int app_config_convert_modem_type(const char *type) {
  if (strcmp(type, "gfsk") == 0) {
    return MODEM_TYPE_GFSK;
  } else if (strcmp(type, "bpsk") == 0) {
    return MODEM_TYPE_BPSK;
  } else if (strcmp(type, "dpsk") == 0) {
    return MODEM_TYPE_DPSK;
  } else if (strcmp(type, "sdpsk") == 0) {
    return MODEM_TYPE_SDPSK;
  } else if (strcmp(type, "oqpsk") == 0) {
    return MODEM_TYPE_OQPSK;
  } else if (strcmp(type, "psk_pm") == 0) {
    return MODEM_TYPE_PSK_PM;
  }
  return -1;
}

static int app_config_convert_framing_type(const char *type) {
  if (strcmp(type, "none") == 0) {
    return FRAMING_TYPE_NONE;
  }
  return -1;
}

static int app_config_convert_direction(const char *direction) {
  if (strcmp(direction, "rx") == 0) {
    return DIRECTION_RX;
  } else if (strcmp(direction, "tx") == 0) {
    return DIRECTION_TX;
  }
  return -1;
}

static int app_config_merge_gfsk_modem_settings(GfskModemSettings *from, GfskModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(GfskModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    gfsk_modem_settings__init(*to);
  }

  GfskModemSettings *settings = *to;
  //TODO need a better way to determine if property was set
  if (from->sample_rate != 0) {
    settings->sample_rate = from->sample_rate;
  }
  if (from->baud_rate != 0) {
    settings->baud_rate = from->baud_rate;
  }
  if (from->center_freq != 0) {
    settings->center_freq = from->center_freq;
  }
  if (from->deviation != 0) {
    settings->deviation = from->deviation;
  }
  if (from->bt != 0) {
    settings->bt = from->bt;
  }
  if (from->bandwidth != 0) {
    settings->bandwidth = from->bandwidth;
  }

  return 0;
}

// bpsk, dpsk and sdpsk all share the same settings shape (PskModemSettings), so config keys and
// cli flags for all three are named with a common "psk" prefix rather than being duplicated per type
static int app_config_merge_psk_modem_settings(PskModemSettings *from, PskModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(PskModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    psk_modem_settings__init(*to);
  }

  PskModemSettings *settings = *to;
  //TODO need a better way to determine if property was set
  if (from->sample_rate != 0) {
    settings->sample_rate = from->sample_rate;
  }
  if (from->baud_rate != 0) {
    settings->baud_rate = from->baud_rate;
  }
  if (from->center_freq != 0) {
    settings->center_freq = from->center_freq;
  }
  if (from->rrc_beta != 0) {
    settings->rrc_beta = from->rrc_beta;
  }
  if (from->rrc_delay != 0) {
    settings->rrc_delay = from->rrc_delay;
  }
  if (from->costas_bandwidth != 0) {
    settings->costas_bandwidth = from->costas_bandwidth;
  }
  if (from->symsync_filter_bank_size != 0) {
    settings->symsync_filter_bank_size = from->symsync_filter_bank_size;
  }
  if (from->bandwidth != 0) {
    settings->bandwidth = from->bandwidth;
  }

  return 0;
}

static int app_config_merge_psk_pm_modem_settings(PskPmModemSettings *from, PskPmModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(PskPmModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    psk_pm_modem_settings__init(*to);
  }

  PskPmModemSettings *settings = *to;
  //TODO need a better way to determine if property was set
  if (from->sample_rate != 0) {
    settings->sample_rate = from->sample_rate;
  }
  if (from->baud_rate != 0) {
    settings->baud_rate = from->baud_rate;
  }
  if (from->center_freq != 0) {
    settings->center_freq = from->center_freq;
  }
  if (from->rrc_beta != 0) {
    settings->rrc_beta = from->rrc_beta;
  }
  if (from->rrc_delay != 0) {
    settings->rrc_delay = from->rrc_delay;
  }
  if (from->costas_bandwidth != 0) {
    settings->costas_bandwidth = from->costas_bandwidth;
  }
  if (from->symsync_filter_bank_size != 0) {
    settings->symsync_filter_bank_size = from->symsync_filter_bank_size;
  }
  if (from->subcarrier_frequency != 0) {
    settings->subcarrier_frequency = from->subcarrier_frequency;
  }
  if (from->modulation_index != 0) {
    settings->modulation_index = from->modulation_index;
  }
  if (from->carrier_pll_bandwidth != 0) {
    settings->carrier_pll_bandwidth = from->carrier_pll_bandwidth;
  }
  if (from->subcarrier_bandwidth != 0) {
    settings->subcarrier_bandwidth = from->subcarrier_bandwidth;
  }

  return 0;
}

static int app_config_load_psk_from_file(config_t *libconfig, PskModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(PskModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    psk_modem_settings__init(*to);
  }

  PskModemSettings *settings = *to;

  const config_setting_t *setting;

  setting = config_lookup(libconfig, "psk_center_freq");
  if (setting != NULL) {
    settings->center_freq = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "psk_sample_rate");
  if (setting != NULL) {
    settings->sample_rate = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "psk_baud_rate");
  if (setting != NULL) {
    settings->baud_rate = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_rrc_beta");
  if (setting != NULL) {
    settings->rrc_beta = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_rrc_delay");
  if (setting != NULL) {
    settings->rrc_delay = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_costas_bandwidth");
  if (setting != NULL) {
    settings->costas_bandwidth = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_symsync_filter_bank_size");
  if (setting != NULL) {
    settings->symsync_filter_bank_size = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_bandwidth");
  if (setting != NULL) {
    settings->bandwidth = (uint32_t) config_setting_get_int(setting);
  }

  return 0;
}

static int app_config_load_gfsk_from_file(config_t *libconfig, GfskModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(GfskModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    gfsk_modem_settings__init(*to);
  }

  GfskModemSettings *settings = *to;

  const config_setting_t *setting;

  setting = config_lookup(libconfig, "gfsk_center_freq");
  if (setting != NULL) {
    settings->center_freq = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "gfsk_sample_rate");
  if (setting != NULL) {
    settings->sample_rate = (uint64_t) config_setting_get_int64(setting);
  }
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
    settings->bt = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "gfsk_use_dc_block");
  if (setting != NULL) {
    settings->use_dc_block = config_setting_get_bool(setting) ? true : false;
  }

  return 0;
}

static int app_config_load_psk_pm_from_file(config_t *libconfig, PskPmModemSettings **to) {
  if (*to == NULL) {
    *to = malloc(sizeof(PskPmModemSettings));
    if (*to == NULL) {
      return -ENOMEM;
    }
    psk_pm_modem_settings__init(*to);
  }

  PskPmModemSettings *settings = *to;

  const config_setting_t *setting;

  setting = config_lookup(libconfig, "psk_pm_center_freq");
  if (setting != NULL) {
    settings->center_freq = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_sample_rate");
  if (setting != NULL) {
    settings->sample_rate = (uint64_t) config_setting_get_int64(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_baud_rate");
  if (setting != NULL) {
    settings->baud_rate = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_rrc_beta");
  if (setting != NULL) {
    settings->rrc_beta = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_rrc_delay");
  if (setting != NULL) {
    settings->rrc_delay = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_costas_bandwidth");
  if (setting != NULL) {
    settings->costas_bandwidth = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_symsync_filter_bank_size");
  if (setting != NULL) {
    settings->symsync_filter_bank_size = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_subcarrier_frequency");
  if (setting != NULL) {
    settings->subcarrier_frequency = (uint32_t) config_setting_get_int(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_modulation_index");
  if (setting != NULL) {
    settings->modulation_index = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_carrier_pll_bandwidth");
  if (setting != NULL) {
    settings->carrier_pll_bandwidth = config_setting_get_float(setting);
  }
  setting = config_lookup(libconfig, "psk_pm_subcarrier_bandwidth");
  if (setting != NULL) {
    settings->subcarrier_bandwidth = (uint32_t) config_setting_get_int(setting);
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

  setting = config_lookup(libconfig, "direction");
  if (setting != NULL) {
    result->direction = app_config_convert_direction(config_setting_get_string(setting));
  }

  setting = config_lookup(libconfig, "sdr_type");
  if (setting != NULL) {
    result->sdr_type = app_config_convert_sdr_type(config_setting_get_string(setting));
  }

  if (result->sdr_type == SDR_TYPE_SDR_SERVER) {
    setting = config_lookup(libconfig, "sdr_server_address");
    if (setting != NULL) {
      result->sdr_server_address = strdup(config_setting_get_string(setting));
      if (result->sdr_server_address == NULL) {
        return -ENOMEM;
      }
    }
    setting = config_lookup(libconfig, "sdr_server_port");
    if (setting != NULL) {
      result->sdr_server_port = config_setting_get_int(setting);
    }
  }
  if (result->sdr_type == SDR_TYPE_PLUTOSDR) {
    setting = config_lookup(libconfig, "plutosdr_gain");
    if (setting != NULL) {
      result->plutosdr_gain = config_setting_get_float(setting);
    }
    setting = config_lookup(libconfig, "plutosdr_timeout_millis");
    if (setting != NULL) {
      result->plutosdr_timeout_millis = config_setting_get_int(setting);
    }
  }
  if (result->sdr_type == SDR_TYPE_FILE) {
    setting = config_lookup(libconfig, "file_format");
    if (setting != NULL) {
      result->file_format = app_config_convert_file_format(config_setting_get_string(setting));
    }
  }

  setting = config_lookup(libconfig, "modem");
  if (setting != NULL) {
    result->modem = app_config_convert_modem_type(config_setting_get_string(setting));
  }
  if (result->modem == MODEM_TYPE_GFSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_GFSK;
    code = app_config_load_gfsk_from_file(libconfig, &result->req.gfsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_BPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_BPSK;
    code = app_config_load_psk_from_file(libconfig, &result->req.bpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_DPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_DPSK;
    code = app_config_load_psk_from_file(libconfig, &result->req.dpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_SDPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_SDPSK;
    code = app_config_load_psk_from_file(libconfig, &result->req.sdpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_OQPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_OQPSK;
    code = app_config_load_psk_from_file(libconfig, &result->req.oqpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_PSK_PM) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_PSK_PM;
    code = app_config_load_psk_pm_from_file(libconfig, &result->req.psk_pm);
    if (code != 0) {
      return code;
    }
  }
  setting = config_lookup(libconfig, "framing");
  if (setting != NULL) {
    result->framing = app_config_convert_framing_type(config_setting_get_string(setting));
    //ignore framing for now
  }

  setting = config_lookup(libconfig, "debug_constellation_file");
  if (setting != NULL) {
    result->debug_constellation_file = strdup(config_setting_get_string(setting));
    if (result->debug_constellation_file == NULL) {
      return -ENOMEM;
    }
  }

  setting = config_lookup(libconfig, "debug_baseband_file");
  if (setting != NULL) {
    result->debug_baseband_file = strdup(config_setting_get_string(setting));
    if (result->debug_baseband_file == NULL) {
      return -ENOMEM;
    }
  }

  setting = config_lookup(libconfig, "debug_subcarrier_file");
  if (setting != NULL) {
    result->debug_subcarrier_file = strdup(config_setting_get_string(setting));
    if (result->debug_subcarrier_file == NULL) {
      return -ENOMEM;
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
    OPT_PLUTOSDR_GAIN,
    OPT_PLUTOSDR_TIMEOUT_MILLIS,
    OPT_FILE,
    OPT_FILE_FORMAT,
    OPT_CONFIG,
    OPT_INPUT,
    OPT_OUTPUT,
    OPT_MODEM,
    OPT_FRAMING,
    OPT_GFSK_CENTER_FREQ,
    OPT_GFSK_SAMPLE_RATE,
    OPT_GFSK_BAUD_RATE,
    OPT_GFSK_DEVIATION,
    OPT_GFSK_BANDWIDTH,
    OPT_GFSK_BT,
    OPT_GFSK_USE_DC_BLOCK,
    OPT_PSK_CENTER_FREQ,
    OPT_PSK_SAMPLE_RATE,
    OPT_PSK_BAUD_RATE,
    OPT_PSK_RRC_BETA,
    OPT_PSK_RRC_DELAY,
    OPT_PSK_COSTAS_BANDWIDTH,
    OPT_PSK_SYMSYNC_FILTER_BANK_SIZE,
    OPT_PSK_BANDWIDTH,
    OPT_PSK_PM_CENTER_FREQ,
    OPT_PSK_PM_SAMPLE_RATE,
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
    {"plutosdr_gain", required_argument, NULL, OPT_PLUTOSDR_GAIN},
    {"plutosdr_timeout_millis", required_argument, NULL, OPT_PLUTOSDR_TIMEOUT_MILLIS},
    {"file", required_argument, NULL, OPT_FILE},
    {"file_format", required_argument, NULL, OPT_FILE_FORMAT},
    {"config", required_argument, NULL, OPT_CONFIG},
    {"input", required_argument, NULL, OPT_INPUT},
    {"output", required_argument, NULL, OPT_OUTPUT},
    {"modem", required_argument, NULL, OPT_MODEM},
    {"framing", required_argument, NULL, OPT_FRAMING},
    {"gfsk_center_freq", required_argument, NULL, OPT_GFSK_CENTER_FREQ},
    {"gfsk_sample_rate", required_argument, NULL, OPT_GFSK_SAMPLE_RATE},
    {"gfsk_baud_rate", required_argument, NULL, OPT_GFSK_BAUD_RATE},
    {"gfsk_deviation", required_argument, NULL, OPT_GFSK_DEVIATION},
    {"gfsk_bandwidth", required_argument, NULL, OPT_GFSK_BANDWIDTH},
    {"gfsk_bt", required_argument, NULL, OPT_GFSK_BT},
    {"gfsk_use_dc_block", required_argument, NULL, OPT_GFSK_USE_DC_BLOCK},
    {"psk_center_freq", required_argument, NULL, OPT_PSK_CENTER_FREQ},
    {"psk_sample_rate", required_argument, NULL, OPT_PSK_SAMPLE_RATE},
    {"psk_baud_rate", required_argument, NULL, OPT_PSK_BAUD_RATE},
    {"psk_rrc_beta", required_argument, NULL, OPT_PSK_RRC_BETA},
    {"psk_rrc_delay", required_argument, NULL, OPT_PSK_RRC_DELAY},
    {"psk_costas_bandwidth", required_argument, NULL, OPT_PSK_COSTAS_BANDWIDTH},
    {"psk_symsync_filter_bank_size", required_argument, NULL, OPT_PSK_SYMSYNC_FILTER_BANK_SIZE},
    {"psk_bandwidth", required_argument, NULL, OPT_PSK_BANDWIDTH},
    {"psk_pm_center_freq", required_argument, NULL, OPT_PSK_PM_CENTER_FREQ},
    {"psk_pm_sample_rate", required_argument, NULL, OPT_PSK_PM_SAMPLE_RATE},
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

  // populate structures opportunistically
  // then discard if different type was selected
  GfskModemSettings gfsk_settings = GFSK_MODEM_SETTINGS__INIT;
  PskModemSettings psk_settings = PSK_MODEM_SETTINGS__INIT;
  PskPmModemSettings psk_pm_settings = PSK_PM_MODEM_SETTINGS__INIT;

  optind = 1;
  opterr = 1;
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
      case OPT_DIRECTION:
        result->direction = app_config_convert_direction(optarg);
        break;
      case OPT_SDR_TYPE:
        result->sdr_type = app_config_convert_sdr_type(optarg);
        break;
      case OPT_SDR_SERVER_ADDRESS: {
        char *sdr_server_address = strdup(optarg);
        if (sdr_server_address == NULL) {
          return -ENOMEM;
        }
        if (result->sdr_server_address != NULL) {
          free(result->sdr_server_address);
        }
        result->sdr_server_address = sdr_server_address;
        break;
      }
      case OPT_SDR_SERVER_PORT:
        result->sdr_server_port = atoi(optarg);
        break;
      case OPT_PLUTOSDR_GAIN:
        result->plutosdr_gain = atof(optarg);
        break;
      case OPT_PLUTOSDR_TIMEOUT_MILLIS:
        result->plutosdr_timeout_millis = (unsigned int) atoi(optarg);
        break;
      case OPT_FILE:
        result->file = strdup(optarg);
        break;
      case OPT_FILE_FORMAT:
        result->file_format = app_config_convert_file_format(optarg);
        break;
      case OPT_INPUT:
        result->input_file = strdup(optarg);
        break;
      case OPT_OUTPUT:
        result->output_file = strdup(optarg);
        break;
      case OPT_MODEM:
        result->modem = app_config_convert_modem_type(optarg);
        break;
      case OPT_FRAMING:
        result->framing = app_config_convert_framing_type(optarg);
        break;
      case OPT_GFSK_CENTER_FREQ:
        gfsk_settings.center_freq = strtoull(optarg, NULL, 10);
        break;
      case OPT_GFSK_SAMPLE_RATE:
        gfsk_settings.sample_rate = strtoull(optarg, NULL, 10);
        break;
      case OPT_GFSK_BAUD_RATE:
        gfsk_settings.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_GFSK_DEVIATION:
        gfsk_settings.deviation = strtoll(optarg, NULL, 10);
        break;
      case OPT_GFSK_BANDWIDTH:
        gfsk_settings.bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_GFSK_BT:
        gfsk_settings.bt = (float) atof(optarg);
        break;
      case OPT_GFSK_USE_DC_BLOCK:
        gfsk_settings.use_dc_block = (strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0);
        break;
      case OPT_PSK_CENTER_FREQ:
        psk_settings.center_freq = strtoull(optarg, NULL, 10);
        break;
      case OPT_PSK_SAMPLE_RATE:
        psk_settings.sample_rate = strtoull(optarg, NULL, 10);
        break;
      case OPT_PSK_BAUD_RATE:
        psk_settings.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_RRC_BETA:
        psk_settings.rrc_beta = (float) atof(optarg);
        break;
      case OPT_PSK_RRC_DELAY:
        psk_settings.rrc_delay = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_COSTAS_BANDWIDTH:
        psk_settings.costas_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_SYMSYNC_FILTER_BANK_SIZE:
        psk_settings.symsync_filter_bank_size = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_BANDWIDTH:
        psk_settings.bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_CENTER_FREQ:
        psk_pm_settings.center_freq = strtoull(optarg, NULL, 10);
        break;
      case OPT_PSK_PM_SAMPLE_RATE:
        psk_pm_settings.sample_rate = strtoull(optarg, NULL, 10);
        break;
      case OPT_PSK_PM_BAUD_RATE:
        psk_pm_settings.baud_rate = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_RRC_BETA:
        psk_pm_settings.rrc_beta = (float) atof(optarg);
        break;
      case OPT_PSK_PM_RRC_DELAY:
        psk_pm_settings.rrc_delay = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_COSTAS_BANDWIDTH:
        psk_pm_settings.costas_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_PM_SYMSYNC_FILTER_BANK_SIZE:
        psk_pm_settings.symsync_filter_bank_size = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_SUBCARRIER_FREQUENCY:
        psk_pm_settings.subcarrier_frequency = (uint32_t) atoi(optarg);
        break;
      case OPT_PSK_PM_MODULATION_INDEX:
        psk_pm_settings.modulation_index = (float) atof(optarg);
        break;
      case OPT_PSK_PM_CARRIER_PLL_BANDWIDTH:
        psk_pm_settings.carrier_pll_bandwidth = (float) atof(optarg);
        break;
      case OPT_PSK_PM_SUBCARRIER_BANDWIDTH:
        psk_pm_settings.subcarrier_bandwidth = (uint32_t) atoi(optarg);
        break;
      case OPT_FREQ_OFFSET_FILE:
        result->freq_offset_file = strdup(optarg);
        break;
      case OPT_DEBUG_FREQ_OFFSET_FILE:
        result->debug_freq_offset_file = strdup(optarg);
        break;
      case OPT_DEBUG_CONSTELLATION_FILE:
        result->debug_constellation_file = strdup(optarg);
        break;
      case OPT_DEBUG_BASEBAND_FILE:
        result->debug_baseband_file = strdup(optarg);
        break;
      case OPT_DEBUG_SUBCARRIER_FILE:
        result->debug_subcarrier_file = strdup(optarg);
        break;
      case OPT_CONFIG:
      default:
        // already handled by app_config_create / unknown option
        break;
    }
  }

  if (result->modem == MODEM_TYPE_GFSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_GFSK;
    int code = app_config_merge_gfsk_modem_settings(&gfsk_settings, &result->req.gfsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_BPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_BPSK;
    int code = app_config_merge_psk_modem_settings(&psk_settings, &result->req.bpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_DPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_DPSK;
    int code = app_config_merge_psk_modem_settings(&psk_settings, &result->req.dpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_SDPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_SDPSK;
    int code = app_config_merge_psk_modem_settings(&psk_settings, &result->req.sdpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_OQPSK) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_OQPSK;
    int code = app_config_merge_psk_modem_settings(&psk_settings, &result->req.oqpsk);
    if (code != 0) {
      return code;
    }
  } else if (result->modem == MODEM_TYPE_PSK_PM) {
    result->req.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_PSK_PM;
    int code = app_config_merge_psk_pm_modem_settings(&psk_pm_settings, &result->req.psk_pm);
    if (code != 0) {
      return code;
    }
  }

  return 0;
}

static void app_config_apply_psk_pm_defaults(PskPmModemSettings *settings) {
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
  if (settings->carrier_pll_bandwidth == 0.0f) {
    settings->carrier_pll_bandwidth = 0.0001f;
  }
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
    if (result->sdr_server_address == NULL) {
      result->sdr_server_address = strdup("127.0.0.1");
    }
    if (result->sdr_server_port == 0) {
      result->sdr_server_port = 8090;
    }
    fprintf(stdout, "sdr: sdr_server\n");
    fprintf(stdout, "sdr_server connection: %s:%d\n", result->sdr_server_address, result->sdr_server_port);
  } else if (result->sdr_type == SDR_TYPE_PLUTOSDR) {
    fprintf(stdout, "sdr: plutosdr\n");
    fprintf(stdout, "plutosdr_gain: %f\n", result->plutosdr_gain);
    if (result->plutosdr_timeout_millis == 0) {
      result->plutosdr_timeout_millis = 10000;
    }
    fprintf(stdout, "plutosdr_timeout_millis: %d\n", result->plutosdr_timeout_millis);
  } else if (result->sdr_type == SDR_TYPE_FILE) {
    fprintf(stdout, "sdr: file\n");
    if (!is_cli_mode) {
      fprintf(stderr, "<3>sdr_type=file is not supported in the server mode\n");
      return -1;
    }
    if (result->file == NULL) {
      fprintf(stderr, "<3>file parameter is missing\n");
      return -1;
    }
    fprintf(stdout, "file: %s\n", result->file);
    if (result->file_format == FILE_FORMAT_GUESS) {
      result->file_format = app_config_guess_file_format(result->file);
    }
    if (result->file_format < 0) {
      fprintf(stderr, "<3>invalid or unable to guess file_format\n");
      return -1;
    }
    fprintf(stdout, "file_format: %s\n", result->file_format == FILE_FORMAT_CU8 ? "cu8" : (result->file_format == FILE_FORMAT_CS16 ? "cs16" : "cf32"));
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
  if (is_cli_mode && result->modem == MODEM_TYPE_NONE) {
    fprintf(stderr, "<3>sdr is enabled, but the modem configuration is missing\n");
    return -1;
  }

  if (result->modem < 0) {
    fprintf(stderr, "<3>invalid modem\n");
    return -1;
  }
  PskModemSettings *psk_settings = NULL;
  switch (result->req.modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
      psk_settings = result->req.bpsk;
      break;
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
      psk_settings = result->req.dpsk;
      break;
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      psk_settings = result->req.sdpsk;
      break;
    case MODEM_REQUEST__MODEM_SETTINGS_OQPSK:
      psk_settings = result->req.oqpsk;
      break;
    default:
      // do nothing
      break;
  }
  if (psk_settings != NULL) {
    if (psk_settings->symsync_filter_bank_size == 0) {
      psk_settings->symsync_filter_bank_size = 32;
    }
    if (psk_settings->rrc_delay == 0) {
      psk_settings->rrc_delay = 5;
    }
    if (psk_settings->rrc_beta == 0.0f) {
      psk_settings->rrc_beta = 0.35f;
    }
    if (psk_settings->costas_bandwidth == 0.0f) {
      psk_settings->costas_bandwidth = 0.01f;
    }
  }
  if (result->req.modem_settings_case == MODEM_REQUEST__MODEM_SETTINGS_PSK_PM) {
    app_config_apply_psk_pm_defaults(result->req.psk_pm);
  }

  if (result->framing < 0) {
    fprintf(stderr, "<3>invalid framing\n");
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
  modem_request__init(&result->req);

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
  if (config->sdr_server_address != NULL) {
    free(config->sdr_server_address);
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
  if (config->file != NULL) {
    free(config->file);
  }
  if (config->input_file != NULL) {
    free(config->input_file);
  }
  if (config->output_file != NULL) {
    free(config->output_file);
  }
  if (config->req.gfsk != NULL) {
    free(config->req.gfsk);
  }
  free(config);
}
