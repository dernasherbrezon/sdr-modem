#ifndef SDR_SERVER_CLIENT_H_
#define SDR_SERVER_CLIENT_H_

#include <complex.h>
#include <stdint.h>
#include "sdr_server_api.h"
#include "sdr_device.h"

typedef struct sdr_server_client_t sdr_server_client;

int sdr_server_client_create2(uint32_t id, struct sdr_worker_rx *rx, char *addr, int port, int read_timeout_seconds, uint32_t max_output_buffer_length, sdr_device **result);

int sdr_server_client_create(uint32_t id, char *addr, int port, int read_timeout_seconds, uint32_t max_output_buffer_length, sdr_server_client **client);

int sdr_server_client_read_stream(float complex **output, size_t *output_len, void *plugin);

int sdr_server_client_request(struct sdr_server_request request, struct sdr_server_response **response, sdr_server_client *client);

void sdr_server_client_stop(void *plugin);

void sdr_server_client_destroy(void *plugin);

#endif /* SDR_SERVER_CLIENT_H_ */
