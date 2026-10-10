#include <stdlib.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <inttypes.h>

#include "api.h"
#include "tcp_server.h"
#include "sdr_rx_worker.h"
#include "sdr_tx_worker.h"
#include "dsp/sdr_modem.h"
#include "sdr/sdr_device.h"
#include "sdr/plutosdr.h"
#include "sdr_utils.h"
#include "tcp_utils.h"

struct tcp_server_t {
  int server_socket;
  volatile sig_atomic_t is_running;
  pthread_t acceptor_thread;
  app_config *app_config;
  uint32_t client_counter;

  bool client_disconnected;
  int client_socket;
  pthread_mutex_t mutex;

  client_tx_worker *tx_worker;
  sdr_rx_worker *sdr_rx_worker;
  sdr_tx_worker *sdr_tx_worker;
  sdr_device *sdr;

  uint8_t *buffer;
  size_t buffer_length;
};

static void log_client(struct sockaddr_in *address, uint32_t id) {
  char str[INET_ADDRSTRLEN];
  const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
  printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

static void *tcp_worker_callback(tcp_server *worker) {
  uint32_t id = worker->client_counter;
  fprintf(stdout, "[%d] tcp_worker is starting\n", id);
  while (worker->is_running && !worker->client_disconnected) {
    int code = tcp_utils_read_data(worker->buffer, API_HEADER_SIZE, worker->client_socket);
    if (code < -1) {
      // read timeout happened. it's ok.
      // client already sent all information we need
      continue;
    }
    if (code == -1) {
      fprintf(stdout, "[%d] client disconnected\n", id);
      client_tx_worker_send_new(-1, worker->tx_worker);
      worker->client_disconnected = true;
      break;
    }
    message_header header;
    code = api_decode_message_header(worker->buffer, API_HEADER_SIZE, &header);
    if (code != 0) {
      //do not log failure attempts, might be ddos from malicious client
      continue;
    }
    if (header.protocol_version != PROTOCOL_VERSION) {
      fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", id, header.protocol_version);
      client_tx_worker_send_response(header.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
      continue;
    }
    if (header.message_length > worker->buffer_length) {
      fprintf(stderr, "message length %"PRIu32" is bigger than allowed %zu\n", header.message_length, worker->buffer_length);
      client_tx_worker_send_response(header.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
      continue;
    }
    code = tcp_utils_read_data(worker->buffer, header.message_length, worker->client_socket);
    if (code != 0) {
      //do not log failure attempts, might be ddos from malicious client
      continue;
    }
    switch (header.type) {
      case MESSAGE_TYPE_RX_COMM_PARAMETERS:
      case MESSAGE_TYPE_TX_COMM_PARAMETERS: {
        // this will validate if settings can be created from the worker->buffer
        // if OK then pass worker->buffer (serialized settings) to the worker thread
        // next step of validation will happen there
        comm_settings settings;
        code = api_decode_comm_settings(worker->buffer, header.message_length, &settings);
        if (code != 0) {
          client_tx_worker_send_response(header.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
          break;
        }

        if (header.type == MESSAGE_TYPE_TX_COMM_PARAMETERS) {
          sdr_tx_worker_set_comm_parameters(header.request_id, worker->buffer, header.message_length, worker->sdr_tx_worker);
        } else if (header.type == MESSAGE_TYPE_RX_COMM_PARAMETERS) {
          sdr_rx_worker_set_comm_parameters(header.request_id, worker->buffer, header.message_length, worker->sdr_rx_worker);
        }
        break;
      }
      case MESSAGE_TYPE_TX_FRAME: {
        if (header.message_length > worker->app_config->max_frame_size) {
          fprintf(stderr, "frame length %"PRIu32" is bigger than max allowed %"PRIu32, header.message_length, worker->app_config->max_frame_size);
          break;
        }
        // frame must be read fully before passing to sdr
        // this will ensure sdr won't underflow
        sdr_tx_worker_send(header.request_id, worker->buffer, header.message_length, worker->sdr_tx_worker);
        break;
      }
      case MESSAGE_TYPE_PING: {
        client_tx_worker_send_response(header.request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_NO_DETAILS, worker->tx_worker);
        break;
      }
      case MESSAGE_TYPE_SHUTDOWN: {
        fprintf(stdout, "[%d] client requested disconnect\n", id);
        client_tx_worker_send_new(-1, worker->tx_worker);
        worker->client_disconnected = true;
        break;
      }
      default: {
        fprintf(stderr, "<3>[%d] unsupported request: %d\n", id, header.type);
        client_tx_worker_send_response(header.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
        break;
      }
    }
  }

  //FIXME stop sdr, stop modem

  close(worker->client_socket);

  worker->is_running = false;
  return (void *) 0;
}

static void *acceptor_worker(void *arg) {
  tcp_server *server = (tcp_server *) arg;
  struct sockaddr_in address;
  while (server->is_running) {
    int addrlen = sizeof(address);
    if ((server->client_socket = accept(server->server_socket, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
      break;
    }

    struct timeval tv;
    tv.tv_sec = server->app_config->read_timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(server->client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv)) {
      close(server->client_socket);
      perror("setsockopt - SO_RCVTIMEO");
      continue;
    }

    // always increment counter to make even error messages traceable
    server->client_counter++;
    server->client_disconnected = false;

    log_client(&address, server->client_counter);

    client_tx_worker_send_new(server->client_socket, server->tx_worker);
    // process worked on the acceptor thread
    // i.e. handle only one client at a time
    tcp_worker_callback(server);
  }

  printf("tcp server stopped\n");
  return (void *) 0;
}

int tcp_server_create(app_config *config, tcp_server **server) {
  tcp_server *result = malloc(sizeof(struct tcp_server_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  *result = (struct tcp_server_t){0};
  result->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == 0) {
    tcp_server_destroy(result);
    return -1;
  }
  result->server_socket = server_socket;
  result->is_running = true;
  result->app_config = config;
  // start counting from 0
  result->client_counter = -1;
  result->buffer_length = config->buffer_size + API_HEADER_SIZE + 100; // 100 is some delta
  result->buffer = malloc(result->buffer_length);
  if (result->buffer == NULL) {
    tcp_server_destroy(result);
    return -ENOMEM;
  }
  int code = sdr_device_create(config, &result->sdr);
  if (code != 0) {
    tcp_server_destroy(result);
    return -1;
  }
  code = client_tx_worker_create(config->buffer_size, config->queue_size, &result->tx_worker);
  if (code != 0) {
    tcp_server_destroy(result);
    return -1;
  }
  code = sdr_rx_worker_create(1, config->buffer_size, result->tx_worker, result->sdr, &result->sdr_rx_worker);
  if (code != 0) {
    tcp_server_destroy(result);
    return -1;
  }
  code = sdr_tx_worker_create(config->max_frame_size, config->buffer_size, config->queue_size, result->tx_worker, result->sdr, &result->sdr_tx_worker);
  if (code != 0) {
    tcp_server_destroy(result);
    return -1;
  }
  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    tcp_server_destroy(result);
    return -1;
  }

#ifdef SO_REUSEPORT
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
    tcp_server_destroy(result);
    return -1;
  }
#endif

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  if (inet_pton(AF_INET, config->bind_address, &address.sin_addr) <= 0) {
    tcp_server_destroy(result);
    fprintf(stderr, "invalid address: %s\n", config->bind_address);
    return -1;
  }
  address.sin_port = htons(config->port);

  if (bind(server_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
    tcp_server_destroy(result);
    perror("bind failed");
    return -1;
  }
  if (listen(server_socket, 3) < 0) {
    tcp_server_destroy(result);
    perror("listen failed");
    return -1;
  }

  pthread_t acceptor_thread;
  code = pthread_create(&acceptor_thread, NULL, &acceptor_worker, result);
  if (code != 0) {
    //FIXME this and above won't destroy allocated threads and memory
    tcp_server_destroy(result);
    return -1;
  }
  result->acceptor_thread = acceptor_thread;

  *server = result;
  return 0;
}

void tcp_server_join_thread(tcp_server *server) {
  pthread_join(server->acceptor_thread, NULL);
  if (server->sdr_rx_worker != NULL) {
    sdr_rx_worker_destroy(server->sdr_rx_worker);
  }
  if (server->sdr_tx_worker != NULL) {
    sdr_tx_worker_destroy(server->sdr_tx_worker);
  }
  if (server->tx_worker != NULL) {
    client_tx_worker_destroy(server->tx_worker);
  }
  if (server->sdr != NULL) {
    sdr_device_destroy(server->sdr);
  }
  if (server->buffer != NULL) {
    free(server->buffer);
  }
  free(server);
}

void tcp_server_destroy(tcp_server *server) {
  if (server == NULL) {
    return;
  }
  fprintf(stdout, "tcp server is stopping\n");
  server->is_running = false;
  // close is not enough to exit from the blocking "accept" method
  // execute shutdown first
  int code = shutdown(server->server_socket, SHUT_RDWR);
  if (code != 0) {
    close(server->server_socket);
  }
}
