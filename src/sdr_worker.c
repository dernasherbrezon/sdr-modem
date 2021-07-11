#include "sdr_worker.h"
#include "linked_list.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "sdr/sdr_server_client.h"

struct sdr_worker_t {
    struct sdr_rx *rx;
    sdr_device *rx_device;
    uint32_t id;
    linked_list *dsp_configs;
    pthread_t sdr_thread;
    pthread_mutex_t mutex;
};

struct array_t {
    float complex *output;
    size_t output_len;
};

void sdr_worker_foreach_put(void *arg, void *data) {
    struct array_t *output = (struct array_t *) arg;
    dsp_worker *worker = (dsp_worker *) data;
    dsp_worker_put(output->output, output->output_len, worker);
}

static void *sdr_worker_callback(void *arg) {
    sdr_worker *worker = (sdr_worker *) arg;
    fprintf(stdout, "[%d] sdr_worker is starting\n", worker->id);
    struct array_t output;
    while (true) {
        int code = worker->rx_device->sdr_process_rx(&output.output, &output.output_len, worker->rx_device->plugin);
        if (code < -1) {
            // read timeout happened. it's ok.
            continue;
        }
        // terminate only when fully read from socket
        if (code != 0 && output.output_len == 0) {
            break;
        }
        pthread_mutex_lock(&worker->mutex);
        linked_list_foreach(&output, &sdr_worker_foreach_put, worker->dsp_configs);
        pthread_mutex_unlock(&worker->mutex);
    }
    //this would close all client sockets
    //and initiate cascade shutdown of sdr and dsp workers
    pthread_mutex_lock(&worker->mutex);
    linked_list_foreach(NULL, &dsp_worker_shutdown, worker->dsp_configs);
    pthread_mutex_unlock(&worker->mutex);
    return (void *) 0;
}

int sdr_worker_create(uint32_t id, struct sdr_rx *rx, sdr_device *rx_device, sdr_worker **worker) {
    struct sdr_worker_t *result = malloc(sizeof(struct sdr_worker_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct sdr_worker_t) {0};

    result->id = id;
    result->dsp_configs = NULL;
    result->rx = rx;
    result->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    result->rx_device = rx_device;

    pthread_t sdr_thread;
    int code = pthread_create(&sdr_thread, NULL, &sdr_worker_callback, result);
    if (code != 0) {
        sdr_worker_destroy(result);
        return code;
    }
    result->sdr_thread = sdr_thread;

    *worker = result;
    return 0;
}

bool sdr_worker_find_closest(void *id, void *data) {
    if (data == NULL) {
        return false;
    }
    struct sdr_rx *rx = (struct sdr_rx *) id;
    sdr_worker *worker = (sdr_worker *) data;
    if (worker->rx->rx_center_freq == rx->rx_center_freq &&
        worker->rx->rx_sampling_freq >= rx->rx_sampling_freq &&
        worker->rx->band_freq == rx->band_freq) {
        return true;
    }
    return false;
}

void sdr_worker_destroy_by_dsp_worker_id(uint32_t id, sdr_worker *worker) {
    if (worker == NULL) {
        return;
    }
    pthread_mutex_lock(&worker->mutex);
    if (worker->dsp_configs != NULL) {
        linked_list_destroy_by_id(&id, &dsp_worker_find_by_id, &worker->dsp_configs);
    }
    bool result = (worker->dsp_configs == NULL);
    pthread_mutex_unlock(&worker->mutex);
    if (result) {
        sdr_worker_destroy(worker);
    }
}

void sdr_worker_destroy(void *data) {
    if (data == NULL) {
        return;
    }
    sdr_worker *worker = (sdr_worker *) data;
    //gracefully shutdown connection
    if (worker->rx_device != NULL) {
        worker->rx_device->stop_rx(worker->rx_device->plugin);
    }
    pthread_join(worker->sdr_thread, NULL);
    //destroy the rx device
    if (worker->rx_device != NULL) {
        worker->rx_device->destroy(worker->rx_device->plugin);
        free(worker->rx_device);
    }

    pthread_mutex_lock(&worker->mutex);
    if (worker->dsp_configs != NULL) {
        linked_list_destroy(worker->dsp_configs);
    }
    pthread_mutex_unlock(&worker->mutex);

    if (worker->rx != NULL) {
        free(worker->rx);
    }
    uint32_t id = worker->id;
    free(worker);
    fprintf(stdout, "[%d] sdr_worker stopped\n", id);
}

int sdr_worker_add_dsp_worker(dsp_worker *worker, sdr_worker *sdr) {
    pthread_mutex_lock(&sdr->mutex);
    int code = linked_list_add(worker, &dsp_worker_destroy, &sdr->dsp_configs);
    pthread_mutex_unlock(&sdr->mutex);
    return code;
}