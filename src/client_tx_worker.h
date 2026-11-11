#ifndef SDR_MODEM_CLIENT_TX_WORKER_H
#define SDR_MODEM_CLIENT_TX_WORKER_H

#include <stdint.h>
#include <stdio.h>
#include "api.h"

typedef enum {
  CLIENT_TX_SOFT_BITS = 0,
  CLIENT_TX_RESPONSE,
  CLIENT_TX_NEW_CLIENT,
  CLIENT_TX_COMPLETE,
  CLIENT_TX_CARRIER_DETECTED,
  CLIENT_TX_CARRIER_LOST
} client_tx_message_type;

typedef struct client_tx_worker_t client_tx_worker;

void client_tx_worker_send_new(int client_socket, client_tx_worker *worker);

void client_tx_worker_send_soft_bits(uint32_t request_id, int32_t frequency_offset, void *buffer, size_t buffer_len, client_tx_worker *worker);

void client_tx_worker_send_response(uint32_t request_id, response_status type, response_details details, client_tx_worker *worker);

void client_tx_worker_send_event(client_tx_message_type event_type, uint32_t request_id, client_tx_worker *worker);

int client_tx_worker_create(uint32_t buffer_size, uint16_t queue_size, client_tx_worker **worker);

void client_tx_worker_destroy(client_tx_worker *worker);

#endif //SDR_MODEM_CLIENT_TX_WORKER_H
