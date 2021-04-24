#include "sdr_worker.h"
#include "linked_list.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "sdr/sdr_server_client.h"

struct sdr_worker_t {
    struct sdr_worker_rx *rx;
    sdr_server_client *client;
    int client_socket;

    linked_list *dsp_configs;
    pthread_t sdr_thread;
};

struct array_t {
    float complex *output;
    size_t output_len;
};

void sdr_worker_foreach(void *arg, void *data) {
    struct array_t *output = (struct array_t *) arg;
    dsp_worker *worker = (dsp_worker *) data;
    dsp_worker_put(output->output, output->output_len, worker);
}

static void *sdr_worker_callback(void *arg) {
    sdr_worker *worker = (sdr_worker *) arg;
    struct sdr_server_request req;
    req.center_freq = worker->rx->rx_center_freq;
    req.band_freq = worker->rx->band_freq;
    req.destination = SDR_SERVER_REQUEST_DESTINATION_SOCKET;
    req.sampling_rate = worker->rx->rx_sampling_freq;
    struct sdr_server_response *response = NULL;
    int code = sdr_server_client_request(req, &response, worker->client);
    if (code != 0) {
        //FIXME respond failure
        //FIXME close socket
        return (void *) 0;
    }

    struct array_t output;
    while (true) {
        code = sdr_server_client_read_stream(&output.output, &output.output_len, worker->client);
        if (code != 0) {
            break;
        }
        linked_list_foreach(&output, &sdr_worker_foreach, worker->dsp_configs);
        //FIXME mutex for iterating dsp_configs

    }
    return (void *) 0;
}

int sdr_worker_create(int client_socket, struct sdr_worker_rx *rx, char *sdr_server_address, int sdr_server_port, uint32_t max_output_buffer_length, sdr_worker **worker) {
    struct sdr_worker_t *result = malloc(sizeof(struct sdr_worker_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct sdr_worker_t) {0};

    result->dsp_configs = NULL;
    result->rx = rx;
    result->client_socket = client_socket;

    int code = sdr_server_client_create(sdr_server_address, sdr_server_port, max_output_buffer_length, &result->client);
    if (code != 0) {
        sdr_worker_destroy(result);
        return code;
    }

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
        worker->rx->rx_destination == rx->rx_destination) {
        return true;
    }
    return false;
}

void sdr_worker_destroy(void *data) {
    sdr_worker *worker = (sdr_worker *) data;
    if (worker->dsp_configs != NULL) {
        linked_list_destroy(worker->dsp_configs);
    }
    pthread_join(worker->sdr_thread, NULL);
    if (worker->rx != NULL) {
        free(worker->rx);
    }
    if (worker->client != NULL) {
        sdr_server_client_destroy(worker->client);
    }
    free(worker);
}

bool sdr_worker_destroy_by_id(void *id, void *data) {
    sdr_worker *worker = (sdr_worker *) data;
    if (worker->dsp_configs == NULL) {
        return false;
    }
    linked_list_destroy_by_id(id, &dsp_worker_find_by_id, &worker->dsp_configs);
    if (worker->dsp_configs == NULL) {
        return true;
    }
    return false;
}

int sdr_worker_add_dsp_worker(dsp_worker *worker, sdr_worker *sdr) {
    return linked_list_add(worker, &dsp_worker_destroy, &sdr->dsp_configs);
}