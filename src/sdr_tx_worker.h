#ifndef SDR_MODEM_SDR_TX_WORKER_H
#define SDR_MODEM_SDR_TX_WORKER_H
#include <stdint.h>
#include <stdio.h>

#include "client_tx_worker.h"
#include "sdr/sdr_device.h"

typedef struct sdr_tx_worker_t sdr_tx_worker;

int sdr_tx_worker_create(uint32_t max_frame_size, uint32_t buffer_size, uint16_t queue_size, client_tx_worker *tx_worker, sdr_device *sdr, sdr_tx_worker **result);

void sdr_tx_worker_send(uint32_t request_id, uint8_t *frame, size_t frame_len, sdr_tx_worker *worker);

void sdr_tx_worker_destroy(sdr_tx_worker *worker);

#endif
