#ifndef SDR_MODEM_DSP_WORKER_H
#define SDR_MODEM_DSP_WORKER_H

#include <stdbool.h>
#include <complex.h>
#include <stdlib.h>

#include "api.h"
#include "server_config.h"

typedef struct dsp_worker_t dsp_worker;

void dsp_worker_destroy(void *data);

bool dsp_worker_find_by_id(void *id, void *data);

void dsp_worker_close_socket(void *arg, void *data);

void dsp_worker_put(float complex *output, size_t output_len, dsp_worker *worker);

int dsp_worker_create(uint32_t id, int client_socket, struct server_config *config, struct request *req, dsp_worker **result);

#endif //SDR_MODEM_DSP_WORKER_H
