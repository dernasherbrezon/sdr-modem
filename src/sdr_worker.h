#ifndef SDR_MODEM_SDR_WORKER_H
#define SDR_MODEM_SDR_WORKER_H

#include <stdbool.h>
#include "dsp_worker.h"

typedef struct sdr_worker_t sdr_worker;

struct sdr_worker_rx {
    uint32_t rx_center_freq;
    uint32_t rx_sampling_freq;
    uint8_t rx_destination;
    uint32_t band_freq;
};

void sdr_worker_destroy(void *data);

bool sdr_worker_find_closest(void *id, void *data);

bool sdr_worker_destroy_by_id(void *id, void *data);

void sdr_worker_destroy_by_dsp_worker_id(uint32_t id, sdr_worker *sdr);

bool sdr_worker_find_by_dsp_id(void *id, void *data);

int sdr_worker_add_dsp_worker(dsp_worker *worker, sdr_worker *sdr);

int sdr_worker_create(uint32_t id, struct sdr_worker_rx *rx, char *sdr_server_address, int sdr_server_port, int read_timeout_seconds, uint32_t max_output_buffer_length, sdr_worker **result);

#endif //SDR_MODEM_SDR_WORKER_H
