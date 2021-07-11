#ifndef SDR_MODEM_SDR_WORKER_H
#define SDR_MODEM_SDR_WORKER_H

#include <stdbool.h>
#include <stdint.h>
#include "dsp_worker.h"
#include "sdr/sdr_device.h"

typedef struct sdr_worker_t sdr_worker;

void sdr_worker_destroy(void *data);

bool sdr_worker_find_closest(void *id, void *data);

void sdr_worker_destroy_by_dsp_worker_id(uint32_t id, sdr_worker *sdr);

int sdr_worker_add_dsp_worker(dsp_worker *worker, sdr_worker *sdr);

int sdr_worker_create(uint32_t id, struct sdr_rx *rx, sdr_device *rx_device, sdr_worker **result);

#endif //SDR_MODEM_SDR_WORKER_H
