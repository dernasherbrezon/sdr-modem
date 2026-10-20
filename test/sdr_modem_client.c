#include "sdr_modem_client.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../src/tcp_utils.h"

// protects the test from a misbehaving server
#define MAX_PAYLOAD_LEN (16 * 1024 * 1024)

struct sdr_modem_client_t {
  int socket;
  uint8_t *payload;
  size_t payload_capacity;
  size_t soft_bits_received;
};

int sdr_modem_client_create(const char *host, uint16_t port, int timeout_seconds, sdr_modem_client **client) {
  sdr_modem_client *result = calloc(1, sizeof(struct sdr_modem_client_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  result->socket = -1;
  result->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (result->socket < 0) {
    sdr_modem_client_destroy(result);
    return -1;
  }
  struct sockaddr_in address = {0};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &address.sin_addr) <= 0) {
    sdr_modem_client_destroy(result);
    return -1;
  }
  struct timeval tv = {.tv_sec = timeout_seconds, .tv_usec = 0};
  if (setsockopt(result->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    sdr_modem_client_destroy(result);
    return -1;
  }
#ifdef SO_NOSIGPIPE
  int opt = 1;
  setsockopt(result->socket, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
  if (connect(result->socket, (struct sockaddr *) &address, sizeof(address)) != 0) {
    sdr_modem_client_destroy(result);
    return -1;
  }
  *client = result;
  return 0;
}

int sdr_modem_client_send(uint8_t protocol_version, uint8_t type, uint32_t request_id, const uint8_t *payload, size_t payload_len, sdr_modem_client *client) {
  message_header header = {
    .protocol_version = protocol_version,
    .type = type,
    .request_id = request_id,
    .message_length = (uint32_t) payload_len
  };
  uint8_t header_buffer[API_HEADER_SIZE];
  size_t written = 0;
  int code = api_encode_message_header(&header, header_buffer, sizeof(header_buffer), &written);
  if (code != 0) {
    return code;
  }
  code = tcp_utils_write_data(header_buffer, written, client->socket);
  if (code != 0 || payload == NULL || payload_len == 0) {
    return code;
  }
  return tcp_utils_write_data(payload, payload_len, client->socket);
}

static int sdr_modem_client_send_settings(message_type type, uint32_t request_id, const comm_settings *settings, sdr_modem_client *client) {
  uint8_t buffer[256];
  size_t written = 0;
  int code = api_encode_comm_settings(settings, buffer, sizeof(buffer), &written);
  if (code != 0) {
    return code;
  }
  return sdr_modem_client_send(PROTOCOL_VERSION, type, request_id, buffer, written, client);
}

int sdr_modem_client_set_rx_parameters(uint32_t request_id, const comm_settings *settings, sdr_modem_client *client) {
  return sdr_modem_client_send_settings(MESSAGE_TYPE_RX_COMM_PARAMETERS, request_id, settings, client);
}

int sdr_modem_client_set_tx_parameters(uint32_t request_id, const comm_settings *settings, sdr_modem_client *client) {
  return sdr_modem_client_send_settings(MESSAGE_TYPE_TX_COMM_PARAMETERS, request_id, settings, client);
}

int sdr_modem_client_rx_start(uint32_t request_id, sdr_modem_client *client) {
  return sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_RX_START, request_id, NULL, 0, client);
}

int sdr_modem_client_rx_stop(uint32_t request_id, sdr_modem_client *client) {
  return sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_RX_STOP, request_id, NULL, 0, client);
}

int sdr_modem_client_ping(uint32_t request_id, sdr_modem_client *client) {
  return sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_PING, request_id, NULL, 0, client);
}

int sdr_modem_client_tx_frame(uint32_t request_id, const uint8_t *frame, size_t frame_len, sdr_modem_client *client) {
  return sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_TX_FRAME, request_id, frame, frame_len, client);
}

int sdr_modem_client_shutdown(uint32_t request_id, sdr_modem_client *client) {
  return sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_SHUTDOWN, request_id, NULL, 0, client);
}

int sdr_modem_client_receive(message_header *header, const uint8_t **payload, sdr_modem_client *client) {
  uint8_t header_buffer[API_HEADER_SIZE];
  int code = tcp_utils_read_data(header_buffer, sizeof(header_buffer), client->socket);
  if (code < -1) {
    return SDR_MODEM_CLIENT_TIMEOUT;
  }
  if (code != 0) {
    return -1;
  }
  if (api_decode_message_header(header_buffer, sizeof(header_buffer), header) != 0) {
    return -1;
  }
  if (header->message_length > MAX_PAYLOAD_LEN) {
    return -1;
  }
  if (header->message_length > client->payload_capacity) {
    uint8_t *grown = realloc(client->payload, header->message_length);
    if (grown == NULL) {
      return -1;
    }
    client->payload = grown;
    client->payload_capacity = header->message_length;
  }
  if (header->message_length > 0 && tcp_utils_read_data(client->payload, header->message_length, client->socket) != 0) {
    return -1;
  }
  if (header->type == MESSAGE_TYPE_SOFT_BITS) {
    client->soft_bits_received += header->message_length;
  }
  *payload = client->payload;
  return 0;
}

int sdr_modem_client_wait_response(uint32_t request_id, response *resp, sdr_modem_client *client) {
  while (1) {
    message_header header;
    const uint8_t *payload = NULL;
    int code = sdr_modem_client_receive(&header, &payload, client);
    if (code != 0) {
      return code;
    }
    if (header.type == MESSAGE_TYPE_RESPONSE && header.request_id == request_id) {
      return api_decode_response(payload, header.message_length, resp);
    }
  }
}

int sdr_modem_client_wait_soft_bits(size_t min_bytes, sdr_modem_client *client) {
  while (client->soft_bits_received < min_bytes) {
    message_header header;
    const uint8_t *payload = NULL;
    int code = sdr_modem_client_receive(&header, &payload, client);
    if (code != 0) {
      return code;
    }
  }
  return 0;
}

size_t sdr_modem_client_soft_bits_received(const sdr_modem_client *client) {
  return client->soft_bits_received;
}

void sdr_modem_client_destroy(sdr_modem_client *client) {
  if (client == NULL) {
    return;
  }
  if (client->socket >= 0) {
    close(client->socket);
  }
  free(client->payload);
  free(client);
}
