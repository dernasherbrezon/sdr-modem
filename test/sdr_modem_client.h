#ifndef SDR_MODEM_SDR_MODEM_CLIENT_H
#define SDR_MODEM_SDR_MODEM_CLIENT_H

#include <stdlib.h>
#include "../src/api.h"

typedef struct sdr_modem_client_t sdr_modem_client;

int sdr_modem_client_create(const char *addr, int port, uint32_t max_buffer_length, sdr_modem_client **client);

int sdr_modem_write_request(struct message_header *header, struct request *req, sdr_modem_client *client);

int sdr_modem_read_stream(int8_t **output, size_t *output_len, sdr_modem_client *client);

int sdr_modem_read_response(struct message_header **response_header, struct response **resp, sdr_modem_client *client);

void sdr_modem_client_destroy(sdr_modem_client *client);

// this will wait until server release all resources, stop all threads and closes connection
void sdr_modem_client_destroy_gracefully(sdr_modem_client *client);


#endif //SDR_MODEM_SDR_MODEM_CLIENT_H
