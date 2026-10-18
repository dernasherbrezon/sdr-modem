#include "sdr_utils.h"

#include "./sdr/plutosdr.h"
#include "./sdr/airspy.h"

int sdr_device_create(app_config *app_config, sdr_device **device) {
  if (app_config->sdr_type == SDR_TYPE_PLUTOSDR) {
    sdr_device *pluto = NULL;
    int code = plutosdr_create(&app_config->plutosdr, app_config->sdr_buffer_len, app_config->iio, &pluto);
    if (code != 0) {
      return code;
    }
    *device = pluto;
  } else if (app_config->sdr_type == SDR_TYPE_AIRSPY) {
    sdr_device *airspy = NULL;
    int code = airspy_create(&app_config->airspy, app_config->sdr_buffer_len, app_config->airspy_lib, &airspy);
    if (code != 0) {
      return code;
    }
    *device = airspy;
  } else if (app_config->sdr_type == SDR_TYPE_FILE) {
    sdr_device *file = NULL;
    int code = sdr_file_create(&app_config->sdr_file, app_config->sdr_buffer_len, &file);
    if (code != 0) {
      return code;
    }
    *device = file;
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
