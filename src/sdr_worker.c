#include "sdr_worker.h"
#include "linked_list.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "sdr/sdr_server_client.h"

struct sdr_worker_t {
    struct sdr_worker_rx *rx;
    sdr_server_client *client;
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
        int code = sdr_server_client_read_stream(&output.output, &output.output_len, worker->client);
        if (code != 0) {
            break;
        }
        pthread_mutex_lock(&worker->mutex);
        linked_list_foreach(&output, &sdr_worker_foreach_put, worker->dsp_configs);
        pthread_mutex_unlock(&worker->mutex);
    }
    //this would close all client sockets
    //and initiate cascade shutdown of sdr and dsp workers
    pthread_mutex_lock(&worker->mutex);
    linked_list_foreach(NULL, &dsp_worker_close_socket, worker->dsp_configs);
    pthread_mutex_unlock(&worker->mutex);
    return (void *) 0;
}

int sdr_worker_create(uint32_t id, struct sdr_worker_rx *rx, char *sdr_server_address, int sdr_server_port, int read_timeout_seconds, uint32_t max_output_buffer_length, sdr_worker **worker) {
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

    int code = sdr_server_client_create(result->id, sdr_server_address, sdr_server_port, read_timeout_seconds, max_output_buffer_length, &result->client);
    if (code != 0) {
        sdr_worker_destroy(result);
        return code;
    }

    struct sdr_server_request req;
    req.center_freq = rx->rx_center_freq;
    req.band_freq = rx->band_freq;
    req.destination = SDR_SERVER_REQUEST_DESTINATION_SOCKET;
    req.sampling_rate = rx->rx_sampling_freq;
    struct sdr_server_response *response = NULL;
    code = sdr_server_client_request(req, &response, result->client);
    if (code != 0) {
        fprintf(stderr, "<3>[%d] unable to send request to sdr server\n", result->id);
        sdr_worker_destroy(result);
        return code;
    }
    if (response->status != SDR_SERVER_RESPONSE_STATUS_SUCCESS) {
        fprintf(stderr, "<3>[%d] request to sdr server rejected: %d\n", result->id, response->details);
        sdr_worker_destroy(result);
        free(response);
        return -1;
    }
    free(response);

    pthread_t sdr_thread;
    code = pthread_create(&sdr_thread, NULL, &sdr_worker_callback, result);
    if (code != 0) {
        sdr_worker_destroy(result);
        return code;
    }
    result->sdr_thread = sdr_thread;

    *worker = result;
    return 0;
}

bool sdr_worker_find_closest(void *id, void *data) {
    struct sdr_worker_rx *rx = (struct sdr_worker_rx *) id;
    sdr_worker *worker = (sdr_worker *) data;
    if (worker->rx->rx_center_freq == rx->rx_center_freq &&
        worker->rx->rx_sampling_freq >= rx->rx_sampling_freq &&
        worker->rx->rx_destination == rx->rx_destination &&
        worker->rx->band_freq == rx->band_freq) {
        return true;
    }
    return false;
}

void sdr_worker_destroy(void *data) {
    if (data == NULL) {
        return;
    }
    sdr_worker *worker = (sdr_worker *) data;
    // terminate reading from sdr server first
    if (worker->client != NULL) {
        sdr_server_client_destroy(worker->client);
    }
    pthread_join(worker->sdr_thread, NULL);

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

bool sdr_worker_find_by_dsp_id(void *id, void *data) {
    sdr_worker *worker = (sdr_worker *) data;
    pthread_mutex_lock(&worker->mutex);
    if (worker->dsp_configs == NULL) {
        return false;
    }
    bool result = (linked_list_find(id, &dsp_worker_find_by_id, worker->dsp_configs) != NULL);
    pthread_mutex_unlock(&worker->mutex);
    return result;
}

bool sdr_worker_destroy_by_id(void *id, void *data) {
    sdr_worker *worker = (sdr_worker *) data;
    pthread_mutex_lock(&worker->mutex);
    if (worker->dsp_configs == NULL) {
        return false;
    }
    linked_list_destroy_by_id(id, &dsp_worker_find_by_id, &worker->dsp_configs);
    bool result = (worker->dsp_configs == NULL);
    pthread_mutex_unlock(&worker->mutex);
    return result;
}

int sdr_worker_add_dsp_worker(dsp_worker *worker, sdr_worker *sdr) {
    pthread_mutex_lock(&sdr->mutex);
    int code = linked_list_add(worker, &dsp_worker_destroy, &sdr->dsp_configs);
    pthread_mutex_unlock(&sdr->mutex);
    return code;
}