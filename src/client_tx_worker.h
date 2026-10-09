#ifndef SDR_MODEM_CLIENT_TX_WORKER_H
#define SDR_MODEM_CLIENT_TX_WORKER_H

#include <stdint.h>
#include <stdio.h>

typedef struct client_tx_worker_t client_tx_worker;

typedef enum {
  SUCCESS = 0,
  FAILURE
} response_status;

void client_tx_worker_send_new(int client_socket, client_tx_worker *worker);

void client_tx_worker_send_soft_bits(uint32_t request_id, void *buffer, size_t buffer_len, client_tx_worker *worker);

void client_tx_worker_send_response(uint32_t request_id, response_status type, uint32_t details, client_tx_worker *worker);

int client_tx_worker_create(uint32_t buffer_size, uint16_t queue_size, client_tx_worker **worker);

void client_tx_worker_destroy(client_tx_worker *worker);

#endif //SDR_MODEM_CLIENT_TX_WORKER_H
