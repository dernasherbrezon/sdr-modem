#include "sdr_rx_worker.h"
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include "dsp/sdr_modem.h"
#include <complex.h>

#include "queue.h"
#include "tcp_utils.h"

typedef enum {
  SDR_RX_COMM_PARAMETERS = 0
} sdr_rx_message_type;

struct sdr_rx_worker_t {
  uint32_t id;
  int client_socket;
  uint32_t buffer_size;

  queue *queue;
  client_tx_worker *tx_worker;
  sdr_device *rx_device;
  sdr_modem *modem;

  pthread_t dsp_thread;
  bool dsp_thread_started;
};

static void *sdr_rx_worker_callback(void *arg) {
  sdr_rx_worker *worker = (sdr_rx_worker *) arg;
  uint32_t id = worker->id;
  fprintf(stdout, "[%d] sdr_rx_worker is starting\n", id);
  float complex *output = NULL;
  size_t output_len = 0;
  uint32_t request_id = 0;
  while (true) {
    int code = worker->rx_device->sdr_process_rx(&output, &output_len, worker->rx_device->plugin);
    if (code < -1) {
      // read timeout happened. it's ok.
      continue;
    }
    // terminate only when fully read from socket
    if (code != 0 && output_len == 0) {
      break;
    }

    int8_t *demod_output = NULL;
    size_t demod_output_len = 0;
    if (worker->modem != NULL) {
      sdr_modem_demodulate(output, output_len, &demod_output, &demod_output_len, worker->modem);
    }
    if (demod_output == NULL) {
      continue;
    }

    client_tx_worker_send_soft_bits(request_id, demod_output, demod_output_len, worker->tx_worker);
    request_id++;

    queue_message message;
    queue_peak(&message, worker->queue);
    if (message.buffer != NULL) {
      switch (message.type) {
        case SDR_RX_COMM_PARAMETERS: {
          sdr_modem_settings settings;
          code = api_decode_sdr_modem_settings(message.buffer, message.buffer_len, &settings);
          if (code != 0) {
            client_tx_worker_send_response(message.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
            break;
          }

          sdr_modem *new_modem = NULL;
          code = sdr_modem_create(&settings, worker->buffer_size, NULL, &new_modem);
          if (code != 0) {
            client_tx_worker_send_response(message.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
            break;
          }

          if (worker->modem != NULL) {
            sdr_modem_destroy(worker->modem);
          }

          worker->modem = new_modem;
          client_tx_worker_send_response(message.request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
          break;
        }
        default: {
          fprintf(stderr, "unknown message type: %d\n", message.type);
          break;
        }
      }
      queue_complete(worker->queue);
    }
  }
  printf("[%d] sdr_rx_worker stopped\n", worker->id);
  return (void *) 0;
}

void sdr_rx_worker_set_comm_parameters(uint32_t request_id, uint8_t *buffer, size_t buffer_len, sdr_rx_worker *worker) {
  queue_message message = {
    .type = SDR_RX_COMM_PARAMETERS,
    .request_id = request_id,
    .buffer_len = buffer_len,
    .buffer = buffer
  };
  queue_put(&message, worker->queue);
}

int sdr_rx_worker_create(uint32_t id, uint32_t buffer_size, client_tx_worker *tx_worker, sdr_device *rx_device, sdr_rx_worker **worker) {
  struct sdr_rx_worker_t *result = malloc(sizeof(struct sdr_rx_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_rx_worker_t){0};
  result->id = id;
  result->rx_device = rx_device;
  result->tx_worker = tx_worker;
  result->buffer_size = buffer_size;

  // start processing
  pthread_t dsp_thread;
  int code = pthread_create(&dsp_thread, NULL, &sdr_rx_worker_callback, result);
  if (code != 0) {
    sdr_rx_worker_destroy(result);
    return -1;
  }
  result->dsp_thread = dsp_thread;
  result->dsp_thread_started = true;

  *worker = result;
  return 0;
}

void sdr_rx_worker_destroy(void *data) {
  if (data == NULL) {
    return;
  }
  sdr_rx_worker *worker = (sdr_rx_worker *) data;
  fprintf(stdout, "[%d] sdr_rx_worker is stopping\n", worker->id);
  if (worker->rx_device != NULL) {
    worker->rx_device->stop_rx(worker->rx_device->plugin);
  }
  if (worker->dsp_thread_started) {
    // wait until thread terminates and only then destroy the worker
    pthread_join(worker->dsp_thread, NULL);
  }
  //destroy the rx device
  if (worker->rx_device != NULL) {
    worker->rx_device->destroy(worker->rx_device->plugin);
    free(worker->rx_device);
  }
  // cleanup everything only when thread terminates
  if (worker->modem != NULL) {
    sdr_modem_destroy(worker->modem);
  }
  free(worker);
}
