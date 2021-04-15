#include "sdr_worker.h"
#include "linked_list.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

struct sdr_worker_t {
    struct sdr_worker_rx *rx;

    linked_list *dsp_configs;
    pthread_t sdr_thread;
};

static void *sdr_worker_callback(void *arg) {
//    core *core = (struct core_t *) arg;
//    uint32_t buffer_size = core->server_config->buffer_size;
    //FIXME
    return (void *) 0;
}

int sdr_worker_create(struct sdr_worker_rx *rx, sdr_worker **worker) {
    struct sdr_worker_t *result = malloc(sizeof(struct sdr_worker_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct sdr_worker_t) {0};

    result->dsp_configs = NULL;
    result->rx = rx;
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