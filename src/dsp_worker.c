#include "dsp_worker.h"
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include "dsp/modem.h"
#include "queue.h"
#include <complex.h>
#include "tcp_utils.h"
#include "api_utils.h"

struct dsp_worker_t {
  uint32_t id;
  int client_socket;

  sdr_modem *modem;

  queue *queue;
  pthread_t dsp_thread;
  FILE *trace_file;
  FILE *demod_file;
};

bool dsp_worker_find_by_id(void *id, void *data) {
  uint32_t config_id = *(uint32_t *) id;
  dsp_worker *worker = (dsp_worker *) data;
  if (worker->id == config_id) {
    return true;
  }
  return false;
}

void dsp_worker_put(float complex *output, size_t output_len, dsp_worker *worker) {
  queue_put(output, output_len, worker->queue);
}

void dsp_worker_shutdown(void *arg, void *data) {
  dsp_worker *worker = (dsp_worker *) data;
  interrupt_waiting_the_data(worker->queue);
}

static void *dsp_worker_callback(void *arg) {
  dsp_worker *worker = (dsp_worker *) arg;
  uint32_t id = worker->id;
  fprintf(stdout, "[%d] dsp_worker is starting\n", id);
  float complex *input = NULL;
  size_t input_len = 0;
  while (true) {
    take_buffer_for_processing(&input, &input_len, worker->queue);
    // poison pill received
    if (input == NULL) {
      break;
    }
    if (worker->trace_file != NULL) {
      size_t n_written = fwrite(input, sizeof(float complex), input_len, worker->trace_file);
      // if disk is full, then terminate the client
      if (n_written < input_len) {
        complete_buffer_processing(worker->queue);
        fprintf(stderr, "<3>[%d] unable to write sdr data\n", id);
        break;
      }
    }
    int8_t *demod_output = NULL;
    size_t demod_output_len = 0;
    if (worker->modem != NULL) {
      modem_demodulate(input, input_len, &demod_output, &demod_output_len, worker->modem);
    }
    if (demod_output == NULL) {
      complete_buffer_processing(worker->queue);
      continue;
    }

    if (worker->demod_file != NULL) {
      size_t n_written = fwrite(demod_output, sizeof(int8_t), demod_output_len, worker->demod_file);
      // if disk is full, then terminate the client
      if (n_written < demod_output_len) {
        complete_buffer_processing(worker->queue);
        fprintf(stderr, "<3>[%d] unable to write demod data\n", id);
        break;
      }
    }
    int code = tcp_utils_write_data((uint8_t *) demod_output, demod_output_len, worker->client_socket);
    complete_buffer_processing(worker->queue);

    if (code != 0) {
      break;
    }
  }
  printf("[%d] dsp_worker stopped\n", worker->id);
  return (void *) 0;
}

int dsp_worker_create(uint32_t id, int client_socket, app_config *server_config, struct ModemRequest *req,
                      dsp_worker **worker) {
  struct dsp_worker_t *result = malloc(sizeof(struct dsp_worker_t));
  if (result == NULL) {
    return -ENOMEM;
  }
  // init all fields with 0 so that destroy_* method would work
  *result = (struct dsp_worker_t){0};
  result->id = id;
  result->client_socket = client_socket;

  int code = modem_create(server_config, req, &result->modem);
  if (code != 0) {
    fprintf(stderr, "<3>[%d] unable to create modem\n", result->id);
    dsp_worker_destroy(result);
    return code;
  }

  // setup queue
  queue *client_queue = NULL;
  code = create_queue(server_config->buffer_size, server_config->queue_size, false, &client_queue);
  if (code != 0) {
    dsp_worker_destroy(result);
    return code;
  }
  result->queue = client_queue;

  // start processing
  pthread_t dsp_thread;
  code = pthread_create(&dsp_thread, NULL, &dsp_worker_callback, result);
  if (code != 0) {
    dsp_worker_destroy(result);
    return -1;
  }
  result->dsp_thread = dsp_thread;

  *worker = result;
  return 0;
}

void dsp_worker_destroy(void *data) {
  if (data == NULL) {
    return;
  }
  dsp_worker *worker = (dsp_worker *) data;
  fprintf(stdout, "[%d] dsp_worker is stopping\n", worker->id);
  if (worker->queue != NULL) {
    interrupt_waiting_the_data(worker->queue);
  }
  if (worker->dsp_thread != NULL) {
    // wait until thread terminates and only then destroy the worker
    pthread_join(worker->dsp_thread, NULL);
  }
  if (worker->queue != NULL) {
    destroy_queue(worker->queue);
  }
  // cleanup everything only when thread terminates
  if (worker->trace_file != NULL) {
    fclose(worker->trace_file);
  }
  if (worker->demod_file != NULL) {
    fclose(worker->demod_file);
  }
  if (worker->modem != NULL) {
    modem_destroy(worker->modem);
  }
  free(worker);
}
