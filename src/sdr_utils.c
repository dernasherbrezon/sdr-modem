#include "sdr_utils.h"

#include "./sdr/plutosdr.h"

int sdr_device_create(app_config *app_config, sdr_device **device) {
  if (app_config->sdr_type == SDR_TYPE_SDR_SERVER) {
    //re-use sdr connections
    //this will allow demodulating different modes using the same data

    // sdr_server_settings settings = app_config->sdr_server;
    // settings.frequency = rx->rx_center_freq;
    // settings.sample_rate = rx->rx_sample_rate;
    // int code = sdr_server_client_create(1, &settings, app_config->buffer_size, device);
    // if (code != 0) {
    //   return -1;
    // }
  } else if (app_config->sdr_type == SDR_TYPE_PLUTOSDR) {
    plutosdr_settings settings = {
      .tx_powerdown = app_config->plutosdr.tx_powerdown,
      .gain_control_mode = app_config->plutosdr.gain_control_mode,
      .timeout_ms = app_config->plutosdr.timeout_ms
    };
    sdr_device *pluto = NULL;
    int code = plutosdr_create(&settings, app_config->buffer_size, app_config->iio, &pluto);
    if (code != 0) {
      return code;
    }
    *device = pluto;
  } else if (app_config->sdr_type == SDR_TYPE_FILE) {
    //FIXME
    // int code = sdr_file_create()
  } else {
    return -1;
  }
  return 0;
}

void sdr_device_destroy(sdr_device *device) {
  if (device == NULL) {
    return;
  }
  device->destroy(device->plugin);
  free(device);
}
