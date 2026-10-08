#include "client_tx_worker.h"

#include <inttypes.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "api.h"
#include "api.pb-c.h"
#include "queue.h"
#include "tcp_utils.h"

struct client_tx_worker_t {
  queue *queue;
  pthread_t thread;

  uint8_t *buffer;
  size_t buffer_len;

  int client_socket;
};

typedef enum {
  CLIENT_TX_SOFT_BITS = 0,
  CLIENT_TX_RESPONSE
} client_tx_message_type;

typedef struct {
  response_status status;
  uint32_t details;
} client_tx_response;

void client_tx_worker_send_soft_bits(uint32_t request_id, void *buffer, size_t buffer_len, client_tx_worker *worker) {
  queue_message message = {
    .type = CLIENT_TX_SOFT_BITS,
    .request_id = request_id,
    .buffer_len = buffer_len,
    .buffer = buffer
  };
  queue_put(&message, worker->queue);
}

void client_tx_worker_send_response(uint32_t request_id, response_status type, uint32_t details, client_tx_worker *worker) {
  client_tx_response response = {
    .status = type,
    .details = details
  };
  queue_message message = {
    .type = CLIENT_TX_RESPONSE,
    .request_id = request_id,
    .buffer_len = sizeof(client_tx_response),
    .buffer = &response
  };
  queue_put(&message, worker->queue);
}

static void *client_tx_worker_callback(void *arg) {
  client_tx_worker *worker = (client_tx_worker *) arg;
  queue_message message;
  message_header header;
  while (true) {
    queue_take(&message, worker->queue);
    // poison pill received
    if (message.buffer == NULL) {
      break;
    }
    switch (message.type) {
      case CLIENT_TX_SOFT_BITS:
        if (worker->client_socket < 0) {
          break;
        }
        TxData tx = TX_DATA__INIT;
        tx.data.len = message.buffer_len;
        tx.data.data = message.buffer;

        header.type = MESSAGE_TYPE_SOFT_SYMBOLS;
        header.protocol_version = PROTOCOL_VERSION;
        header.request_id = htonl(message.request_id);
        header.message_length = htonl(tx_data__get_packed_size(&tx));
        tcp_utils_write_data((uint8_t *) &header, sizeof(message_header), worker->client_socket);

        size_t expected_len = header.message_length;
        if (expected_len > worker->buffer_len) {
          void *new_buffer = realloc(worker->buffer, expected_len);
          if (new_buffer == NULL) {
            fprintf(stderr, "no memory to send %d.%"PRIu32". cant allocate %zu\n", message.type, message.request_id, expected_len);
            break;
          }
          worker->buffer = new_buffer;
          worker->buffer_len = expected_len;
        }

        tx_data__pack(&tx, worker->buffer);
        tcp_utils_write_data(worker->buffer, header.message_length, worker->client_socket);
        break;
      case CLIENT_TX_RESPONSE:
        if (worker->client_socket < 0) {
          break;
        }
        client_tx_response *tx_response = (client_tx_response *) message.buffer;
        Response response = RESPONSE__INIT;
        response.details = tx_response->details;
        response.status = (ResponseStatus) tx_response->status; // designed to be compatible

        header.type = MESSAGE_TYPE_RESPONSE;
        header.protocol_version = PROTOCOL_VERSION;
        header.request_id = htonl(message.request_id);
        header.message_length = htonl(response__get_packed_size(&response));
        tcp_utils_write_data((uint8_t *) &header, sizeof(message_header), worker->client_socket);

        tcp_utils_write_data(message.buffer, header.message_length, worker->client_socket);
        response__pack(&response, worker->buffer);
        tcp_utils_write_data(worker->buffer, header.message_length, worker->client_socket);
        break;
      default:
        fprintf(stderr, "unknown message type: %d\n", message.type);
        break;
    }

    queue_complete(worker->queue);
  }
  return (void *) 0;
}

int client_tx_worker_create(uint32_t buffer_size, uint16_t queue_size, client_tx_worker **worker) {
  struct client_tx_worker_t *result = malloc(sizeof(struct client_tx_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct client_tx_worker_t){0};
  result->client_socket = -1;

  int code = create_queue(buffer_size, queue_size, false, &result->queue);
  if (code != 0) {
    client_tx_worker_destroy(result);
    return code;
  }

  // max queue buffer + message header + some bytes for protobuf packing
  // buffer will be reallocated anyway if overrun
  result->buffer_len = buffer_size + sizeof(message_header) + 100;
  result->buffer = malloc(result->buffer_len);
  if (result->buffer == NULL) {
    client_tx_worker_destroy(result);
    return -ENOMEM;
  }

  // start processing
  code = pthread_create(&result->thread, NULL, &client_tx_worker_callback, result);
  if (code != 0) {
    client_tx_worker_destroy(result);
    return code;
  }

  *worker = result;
  return 0;
}

void client_tx_worker_destroy(client_tx_worker *worker) {
  if (worker == NULL) {
    return;
  }
  if (worker->queue != NULL) {
    interrupt_waiting_the_data(worker->queue);
  }
  if (worker->thread != NULL) {
    // wait until thread terminates and only then destroy remaining objects
    pthread_join(worker->thread, NULL);
  }
  if (worker->queue != NULL) {
    destroy_queue(worker->queue);
  }
  free(worker);
}
