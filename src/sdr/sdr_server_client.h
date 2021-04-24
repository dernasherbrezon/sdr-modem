#ifndef SDR_SERVER_CLIENT_H_
#define SDR_SERVER_CLIENT_H_

#include <complex.h>
#include "sdr_server_api.h"

typedef struct sdr_server_client_t sdr_server_client;

int sdr_server_client_create(char *addr, int port, uint32_t max_output_buffer_length, sdr_server_client **client);

int sdr_server_client_read_stream(float complex **output, size_t *output_len, sdr_server_client *client);

int sdr_server_client_request(struct sdr_server_request request, struct sdr_server_response **response, sdr_server_client *client);

void sdr_server_client_destroy(sdr_server_client *client);

#endif /* SDR_SERVER_CLIENT_H_ */
