#include "sdr_tx_worker.h"

#include "queue.h"
#include <pthread.h>
#include <errno.h>

#include "api.h"
#include "dsp/sdr_modem.h"

typedef enum {
  SDR_TX_FRAME = 0,
  SDR_TX_COMM_PARAMETERS = 1
} sdr_tx_message_type;

struct sdr_tx_worker_t {
  queue *queue;
  pthread_t thread;
  bool thread_started;

  uint32_t buffer_size;
  client_tx_worker *tx_worker;
  sdr_modem *modem;
  sdr_device *sdr;
};

static void *sdr_tx_worker_callback(void *arg) {
  sdr_tx_worker *worker = (sdr_tx_worker *) arg;
  queue_message message;
  while (true) {
    queue_take(&message, worker->queue);
    // poison pill received
    if (message.buffer == NULL) {
      break;
    }

    int code = 0;
    size_t written = 0;
    switch (message.type) {
      case SDR_TX_FRAME: {
        if (worker->modem == NULL) {
          break;
        }

        float complex *output = NULL;
        size_t output_len = 0;
        sdr_modem_modulate(message.buffer, message.buffer_len, &output, &output_len, worker->modem);

        code = worker->sdr->sdr_process_tx(output, output_len, worker->sdr->plugin);
        if (code != 0) {
          client_tx_worker_send_response(message.request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
          break;
        }

        client_tx_worker_send_response(message.request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_INVALID_REQUEST, worker->tx_worker);
        break;
      }
      case SDR_TX_COMM_PARAMETERS: {
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
  }
  return (void *) 0;
}

int sdr_tx_worker_create(uint32_t max_frame_size, uint32_t buffer_size, uint16_t queue_size, client_tx_worker *tx_worker, sdr_device *sdr, sdr_tx_worker **worker) {
  struct sdr_tx_worker_t *result = malloc(sizeof(struct sdr_tx_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_tx_worker_t){0};
  result->sdr = sdr;
  result->tx_worker = tx_worker;
  result->buffer_size = buffer_size;

  int code = create_queue(max_frame_size, queue_size, true, &result->queue);
  if (code != 0) {
    sdr_tx_worker_destroy(result);
    return code;
  }

  // start processing
  code = pthread_create(&result->thread, NULL, &sdr_tx_worker_callback, result);
  if (code != 0) {
    sdr_tx_worker_destroy(result);
    return code;
  }
  result->thread_started = true;

  *worker = result;
  return 0;
}

void sdr_tx_worker_send(uint32_t request_id, uint8_t *frame, size_t frame_len, sdr_tx_worker *worker) {
  queue_message message = {
    .type = SDR_TX_FRAME,
    .request_id = request_id,
    .buffer_len = frame_len,
    .buffer = frame
  };
  queue_put(&message, worker->queue);
}

void sdr_tx_worker_destroy(sdr_tx_worker *worker) {
  if (worker == NULL) {
    return;
  }
  if (worker->queue != NULL) {
    queue_interrupt(worker->queue);
  }
  if (worker->thread_started) {
    // wait until thread terminates and only then destroy remaining objects
    pthread_join(worker->thread, NULL);
  }
  if (worker->queue != NULL) {
    destroy_queue(worker->queue);
  }
  if (worker->modem != NULL) {
    sdr_modem_destroy(worker->modem);
  }
  free(worker);
}
