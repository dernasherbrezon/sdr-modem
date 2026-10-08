#ifndef SDR_MODEM_DSP_WORKER_H
#define SDR_MODEM_DSP_WORKER_H

#include <stdbool.h>
#include <complex.h>

#include "app_config.h"
#include "client_tx_worker.h"
#include "sdr/sdr_device.h"

typedef struct dsp_worker_t dsp_worker;

int dsp_worker_create(uint32_t id, client_tx_worker *tx_worker, sdr_device *rx_device, dsp_worker **result);

void dsp_worker_destroy(void *data);

#endif //SDR_MODEM_DSP_WORKER_H
