#include "sdr_rx_worker.h"
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include "dsp/sdr_modem.h"
#include <complex.h>

#include "queue.h"
#include "tcp_utils.h"
#include "log.h"

#define SDR_RX_MESSAGE_QUEUE_SIZE 8

typedef enum {
  SDR_RX_COMM_PARAMETERS = 0,
  SDR_RX_START = 1,
  SDR_RX_STOP = 2
} sdr_rx_message_type;

struct sdr_rx_worker_t {
  uint32_t sdr_buffer_len;

  queue *queue;
  client_tx_worker *tx_worker;
  sdr_device *sdr;
  sdr_modem *modem;

  // accessed only from the dsp thread
  bool rx_enabled;

  pthread_t thread;
  bool thread_started;
};

static void sdr_rx_worker_handle_message(const queue_message *message, sdr_rx_worker *worker) {
  switch (message->type) {
    case SDR_RX_COMM_PARAMETERS: {
      comm_settings settings;
      int code = api_decode_comm_settings(message->buffer, message->buffer_len, &settings);
      if (code != 0) {
        client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
        break;
      }
      sdr_modem *new_modem = NULL;
      code = sdr_modem_create(&settings.modem_settings, worker->sdr_buffer_len, NULL, &new_modem);
      if (code != 0) {
        client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
        break;
      }
      code = worker->sdr->set_rx_parameters(&settings.sdr_settings, worker->sdr->plugin);
      if (code != 0) {
        sdr_modem_destroy(new_modem);
        client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
        break;
      }
      if (worker->modem != NULL) {
        sdr_modem_destroy(worker->modem);
      }
      worker->modem = new_modem;
      client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_NO_DETAILS, worker->tx_worker);
      break;
    }
    case SDR_RX_START: {
      if (!worker->rx_enabled) {
        worker->sdr->start_rx(worker->sdr->plugin);
        worker->rx_enabled = true;
      }
      client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_NO_DETAILS, worker->tx_worker);
      break;
    }
    case SDR_RX_STOP: {
      if (worker->rx_enabled) {
        worker->sdr->stop_rx(worker->sdr->plugin);
        worker->rx_enabled = false;
      }
      client_tx_worker_send_response(message->request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_NO_DETAILS, worker->tx_worker);
      break;
    }
    default: {
      LOG_ERROR("unknown message type: %d", message->type);
      break;
    }
  }
}

static void *sdr_rx_worker_callback(void *arg) {
  sdr_rx_worker *worker = (sdr_rx_worker *) arg;
  LOG_DEBUG("sdr_rx_worker is starting");
  float complex *output = NULL;
  size_t output_len = 0;
  uint32_t request_id = 0;
  while (true) {
    queue_message message;
    if (worker->rx_enabled) {
      queue_peak(&message, worker->queue);
    } else {
      // rx is off: nothing to do except waiting for the next command
      queue_take(&message, worker->queue);
      if (message.buffer == NULL) {
        // queue interrupted
        break;
      }
    }
    if (message.buffer != NULL) {
      sdr_rx_worker_handle_message(&message, worker);
      queue_complete(worker->queue);
    }
    if (!worker->rx_enabled) {
      continue;
    }

    int code = worker->sdr->sdr_process_rx(&output, &output_len, worker->sdr->plugin);
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
  }
  LOG_DEBUG("sdr_rx_worker stopped");
  return (void *) 0;
}

static void sdr_rx_worker_put_empty(sdr_rx_message_type type, uint32_t request_id, sdr_rx_worker *worker) {
  uint8_t empty = 0;
  queue_message message = {
    .type = type,
    .request_id = request_id,
    .buffer_len = 0,
    .buffer = &empty
  };
  queue_put(&message, worker->queue);
}

void sdr_rx_worker_start(uint32_t request_id, sdr_rx_worker *worker) {
  sdr_rx_worker_put_empty(SDR_RX_START, request_id, worker);
}

void sdr_rx_worker_stop(uint32_t request_id, sdr_rx_worker *worker) {
  sdr_rx_worker_put_empty(SDR_RX_STOP, request_id, worker);
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

int sdr_rx_worker_create(uint32_t sdr_buffer_len, client_tx_worker *tx_worker, sdr_device *sdr, sdr_rx_worker **worker) {
  struct sdr_rx_worker_t *result = malloc(sizeof(struct sdr_rx_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_rx_worker_t){0};
  result->sdr = sdr;
  result->tx_worker = tx_worker;
  result->sdr_buffer_len = sdr_buffer_len;

  int code = create_queue(sdr_buffer_len, SDR_RX_MESSAGE_QUEUE_SIZE, true, &result->queue);
  if (code != 0) {
    sdr_rx_worker_destroy(result);
    return -1;
  }

  // start processing
  code = pthread_create(&result->thread, NULL, &sdr_rx_worker_callback, result);
  if (code != 0) {
    sdr_rx_worker_destroy(result);
    return -1;
  }
  result->thread_started = true;

  *worker = result;
  return 0;
}

void sdr_rx_worker_destroy(void *data) {
  if (data == NULL) {
    return;
  }
  sdr_rx_worker *worker = (sdr_rx_worker *) data;
  LOG_DEBUG("sdr_rx_worker is stopping");
  if (worker->sdr != NULL) {
    worker->sdr->stop_rx(worker->sdr->plugin);
  }
  if (worker->queue != NULL) {
    queue_interrupt(worker->queue);
  }
  if (worker->thread_started) {
    // wait until thread terminates and only then destroy the worker
    pthread_join(worker->thread, NULL);
  }
  // cleanup everything only when thread terminates
  if (worker->modem != NULL) {
    sdr_modem_destroy(worker->modem);
  }
  if (worker->queue != NULL) {
    destroy_queue(worker->queue);
  }
  free(worker);
}
