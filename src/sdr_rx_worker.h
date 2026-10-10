#ifndef SDR_MODEM_SDR_RX_WORKER_H
#define SDR_MODEM_SDR_RX_WORKER_H

#include <stdbool.h>
#include <complex.h>

#include "app_config.h"
#include "client_tx_worker.h"
#include "sdr/sdr_device.h"

typedef struct sdr_rx_worker_t sdr_rx_worker;

int sdr_rx_worker_create(uint32_t id, client_tx_worker *tx_worker, sdr_device *rx_device, sdr_rx_worker **result);

void sdr_rx_worker_destroy(void *data);

#endif //SDR_MODEM_SDR_RX_WORKER_H
