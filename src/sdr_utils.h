#ifndef SDR_UTILS_H_
#define SDR_UTILS_H_

#include "app_config.h"
#include "sdr/sdr_device.h"

int sdr_device_create(app_config *app_config, sdr_device **device);

void sdr_device_destroy(sdr_device *device);


#endif
