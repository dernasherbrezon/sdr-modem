#include "sdr_rx_worker.h"
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include "dsp/sdr_modem.h"
#include <complex.h>
#include "tcp_utils.h"

struct sdr_rx_worker_t {
  uint32_t id;
  int client_socket;

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
  }
  printf("[%d] sdr_rx_worker stopped\n", worker->id);
  return (void *) 0;
}

int sdr_rx_worker_create(uint32_t id, client_tx_worker *tx_worker, sdr_device *rx_device, sdr_rx_worker **worker) {
  struct sdr_rx_worker_t *result = malloc(sizeof(struct sdr_rx_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct sdr_rx_worker_t){0};
  result->id = id;
  result->rx_device = rx_device;
  result->tx_worker = tx_worker;

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
