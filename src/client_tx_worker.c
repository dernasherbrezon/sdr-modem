#include "client_tx_worker.h"

#include <inttypes.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include "api.h"
#include "queue.h"
#include "tcp_utils.h"

struct client_tx_worker_t {
  queue *queue;
  pthread_t thread;

  uint8_t *header_buffer;
  size_t header_buffer_len;

  uint8_t *buffer;
  size_t buffer_len;

  int client_socket;
};

typedef enum {
  CLIENT_TX_SOFT_BITS = 0,
  CLIENT_TX_RESPONSE,
  CLIENT_TX_NEW_CLIENT
} client_tx_message_type;

void client_tx_worker_send_new(int client_socket, client_tx_worker *worker) {
  queue_message message = {
    .type = CLIENT_TX_NEW_CLIENT,
    .request_id = 0,
    .buffer_len = sizeof(int),
    .buffer = &client_socket
  };
  queue_put(&message, worker->queue);
}

void client_tx_worker_send_soft_bits(uint32_t request_id, void *buffer, size_t buffer_len, client_tx_worker *worker) {
  queue_message message = {
    .type = CLIENT_TX_SOFT_BITS,
    .request_id = request_id,
    .buffer_len = buffer_len,
    .buffer = buffer
  };
  queue_put(&message, worker->queue);
}

void client_tx_worker_send_response(uint32_t request_id, response_status type, response_details details, client_tx_worker *worker) {
  response resp = {
    .status = type,
    .details = details
  };
  queue_message message = {
    .type = CLIENT_TX_RESPONSE,
    .request_id = request_id,
    .buffer_len = sizeof(response),
    .buffer = &resp
  };
  queue_put(&message, worker->queue);
}

static int client_tx_send_header(message_header *header, client_tx_worker *worker) {
  size_t written = 0;
  int code = api_encode_message_header(header, worker->header_buffer, worker->header_buffer_len, &written);
  if (code != 0) {
    return code;
  }
  code = tcp_utils_write_data(worker->header_buffer, written, worker->client_socket);
  if (code != 0) {
    return code;
  }
  return 0;
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
    int code = 0;
    size_t written = 0;
    switch (message.type) {
      case CLIENT_TX_SOFT_BITS:
        if (worker->client_socket < 0) {
          break;
        }
        header.type = MESSAGE_TYPE_SOFT_BITS;
        header.protocol_version = PROTOCOL_VERSION;
        header.request_id = message.request_id;
        header.message_length = message.buffer_len;
        code = client_tx_send_header(&header, worker);
        if (code != 0) {
          break;
        }
        tcp_utils_write_data(message.buffer, header.message_length, worker->client_socket);
        break;
      case CLIENT_TX_RESPONSE:
        if (worker->client_socket < 0) {
          break;
        }
        response *resp = (response *) message.buffer;

        code = api_encode_response(resp, worker->buffer, worker->buffer_len, &written);
        if (code != 0) {
          break;
        }

        header.type = MESSAGE_TYPE_RESPONSE;
        header.protocol_version = PROTOCOL_VERSION;
        header.request_id = message.request_id;
        header.message_length = written;
        code = client_tx_send_header(&header, worker);
        if (code != 0) {
          break;
        }
        code = tcp_utils_write_data(worker->buffer, written, worker->client_socket);
        if (code != 0) {
          break;
        }
        break;
      case CLIENT_TX_NEW_CLIENT:
        memcpy(&worker->client_socket, message.buffer, sizeof(int));
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

  int code = create_queue(buffer_size, queue_size, true, &result->queue);
  if (code != 0) {
    client_tx_worker_destroy(result);
    return code;
  }

  // max queue buffer + message header + some bytes for packing
  result->buffer_len = buffer_size + sizeof(message_header) + 100;
  result->buffer = malloc(result->buffer_len);
  if (result->buffer == NULL) {
    client_tx_worker_destroy(result);
    return -ENOMEM;
  }
  result->header_buffer_len = API_HEADER_SIZE;
  result->header_buffer = malloc(result->header_buffer_len);
  if (result->header_buffer == NULL) {
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
    queue_interrupt(worker->queue);
  }
  if (worker->thread != NULL) {
    // wait until thread terminates and only then destroy remaining objects
    pthread_join(worker->thread, NULL);
  }
  if (worker->queue != NULL) {
    destroy_queue(worker->queue);
  }
  if (worker->buffer != NULL) {
    free(worker->buffer);
  }
  if (worker->header_buffer != NULL) {
    free(worker->header_buffer);
  }
  free(worker);
}
