#ifndef SDR_MODEM_SDR_MODEM_CLIENT_H
#define SDR_MODEM_SDR_MODEM_CLIENT_H

#include <stdlib.h>
#include "../src/api.h"
#include "../src/api.pb-c.h"

typedef struct sdr_modem_client_t sdr_modem_client;

int sdr_modem_client_create(const char *addr, int port, uint32_t max_buffer_length, int read_timeout_seconds, sdr_modem_client **client);

int sdr_modem_client_write_raw(uint8_t *buffer, size_t buffer_len, sdr_modem_client *client);

int sdr_modem_client_write_request(struct message_header *header, struct RxRequest *req, sdr_modem_client *client);

int sdr_modem_client_write_tx_request(struct message_header *header, struct TxRequest *req, sdr_modem_client *client);

int sdr_modem_client_write_tx(struct message_header *header, struct TxData *req, sdr_modem_client *client);

int sdr_modem_client_write_tx_raw(struct message_header *header, struct TxData *req, uint32_t req_len, sdr_modem_client *client);

int sdr_modem_client_read_stream(int8_t **output, size_t expected_read, sdr_modem_client *client);

int sdr_modem_client_read_response(struct message_header **response_header, struct Response **resp, sdr_modem_client *client);

void sdr_modem_client_destroy(sdr_modem_client *client);

// this will wait until server release all resources, stop all threads and closes connection
void sdr_modem_client_destroy_gracefully(sdr_modem_client *client);


#endif //SDR_MODEM_SDR_MODEM_CLIENT_H
