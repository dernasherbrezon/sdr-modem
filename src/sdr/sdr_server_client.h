#ifndef SDR_SERVER_CLIENT_H_
#define SDR_SERVER_CLIENT_H_

#include "sdr_server_api.h"

typedef struct sdr_server_client_t sdr_server_client;

int sdr_server_client_create(char *addr, int port, sdr_server_client **client);

int sdr_server_client_request(struct sdr_server_message_header header, struct sdr_server_request request, struct sdr_server_response **response, sdr_server_client *client);

void sdr_server_client_destroy(sdr_server_client *client);

#endif /* SDR_SERVER_CLIENT_H_ */
