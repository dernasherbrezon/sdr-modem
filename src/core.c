#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <complex.h>
#include <unistd.h>

#include "core.h"
#include "api.h"
#include "linked_list.h"
#include "dsp/fsk_demod.h"

struct dsp_config {
    struct client_config *config;
    fsk_demod *fsk_demod;
    queue *queue;
    pthread_t dsp_thread;
    FILE *file;
};

struct sdr_config {
    uint32_t rx_center_freq;
    uint32_t rx_sampling_freq;
    uint8_t rx_destination;

    linked_list *dsp_configs;
    pthread_t sdr_thread;
};

struct core_t {
    struct server_config *server_config;
    pthread_mutex_t mutex;

    linked_list *sdr_configs;
};

int core_create(struct server_config *server_config, core **result) {
    core *core = malloc(sizeof(struct core_t));
    if (core == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *core = (struct core_t) {0};

    core->server_config = server_config;
    core->sdr_configs = NULL;
    core->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    *result = core;
    return 0;
}

int write_to_file(struct dsp_config *config_node, int8_t *filter_output, size_t filter_output_len) {
    size_t n_written;
    if (config_node->file != NULL) {
        n_written = fwrite(filter_output, sizeof(int8_t), filter_output_len, config_node->file);
    } else {
        fprintf(stderr, "<3>unknown file output\n");
        return -1;
    }
    // if disk is full, then terminate the client
    if (n_written < filter_output_len) {
        return -1;
    } else {
        return 0;
    }
}

int write_to_socket(struct dsp_config *config_node, int8_t *filter_output, size_t filter_output_len) {
    size_t total_len = filter_output_len * sizeof(int8_t);
    size_t left = total_len;
    while (left > 0) {
        int written = write(config_node->config->client_socket, (char *) filter_output + (total_len - left), left);
        if (written < 0) {
            return -1;
        }
        left -= written;
    }
    return 0;
}

static void *dsp_worker(void *arg) {
    struct dsp_config *config_node = (struct dsp_config *) arg;
    fprintf(stdout, "[%d] dsp_worker is starting\n", config_node->config->id);
    float complex *input = NULL;
    size_t input_len = 0;
    int8_t *demod_output = NULL;
    size_t demod_output_len = 0;
    while (true) {
        take_buffer_for_processing(&input, &input_len, config_node->queue);
        // poison pill received
        if (input == NULL) {
            break;
        }
        //FIXME correct doppler
        if (config_node->fsk_demod != NULL) {
            fsk_demod_process(input, input_len, &demod_output, &demod_output_len, config_node->fsk_demod);
        }
        if (demod_output == NULL) {
            // shouldn't happen
            // demod type checked on client request
            continue;
        }
        int code;
        if (config_node->config->req->rx_destination == REQUEST_RX_DESTINATION_FILE) {
            code = write_to_file(config_node, demod_output, demod_output_len);
        } else if (config_node->config->req->rx_destination == REQUEST_RX_DESTINATION_SOCKET) {
            code = write_to_socket(config_node, demod_output, demod_output_len);
        } else {
            fprintf(stderr, "<3>unknown destination: %d\n", config_node->config->req->rx_destination);
            code = -1;
        }

        complete_buffer_processing(config_node->queue);

        if (code != 0) {
            // this would trigger client disconnect
            // I could use "break" here, but "continue" is a bit better:
            //   - single route for abnormal termination (i.e. disk space issue) and normal (i.e. client disconnected)
            //   - all shutdown sequences have: stop tcp thread, then dsp thread, then rtlsdr thread
            //   - processing the queue and writing to the already full disk is OK (I hope)
            //   - calling "close" socket multiple times is OK (I hope)
            close(config_node->config->client_socket);
        }

    }
    destroy_queue(config_node->queue);
    printf("[%d] dsp_worker stopped\n", config_node->config->id);
    return (void *) 0;
}

static void *sdr_worker(void *arg) {
    core *core = (struct core_t *) arg;
    uint32_t buffer_size = core->server_config->buffer_size;
    //FIXME
    return (void *) 0;
}

void dsp_config_destroy(void *data) {
    if (data == NULL) {
        return;
    }
    struct dsp_config *node = (struct dsp_config *) data;
    fprintf(stdout, "[%d] dsp_worker is stopping\n", node->config->id);
    if (node->queue != NULL) {
        interrupt_waiting_the_data(node->queue);
    }
    // wait until thread terminates and only then destroy the node
    pthread_join(node->dsp_thread, NULL);
    // cleanup everything only when thread terminates
    if (node->file != NULL) {
        fclose(node->file);
    }
    if (node->fsk_demod != NULL) {
        fsk_demod_destroy(node->fsk_demod);
    }
    free(node);
}

bool core_find_closest_sdr_config(void *id, void *data) {
    struct client_config *config = (struct client_config *) id;
    struct sdr_config *sdr = (struct sdr_config *) data;
    if (sdr->rx_center_freq == config->req->rx_center_freq && sdr->rx_sampling_freq >= config->req->rx_sampling_freq &&
        sdr->rx_destination == config->req->rx_destination) {
        return true;
    }
    return false;
}

int core_add_client(struct client_config *config) {
    if (config == NULL) {
        return -1;
    }
    struct dsp_config *result = malloc(sizeof(struct dsp_config));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct dsp_config) {0};
    result->config = config;

    //FIXME setup doppler correction

    int code;
    if (config->req->demod_type == REQUEST_DEMOD_TYPE_FSK) {
        code = fsk_demod_create(config->req->rx_sampling_freq, config->req->demod_baud_rate,
                                config->req->demod_fsk_deviation, config->req->demod_decimation,
                                config->req->demod_fsk_transition_width, config->req->demod_fsk_use_dc_block,
                                config->core->server_config->buffer_size, &result->fsk_demod);
    }

    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32", config->core->server_config->base_path, config->id);
    result->file = fopen(file_path, "wb");
    if (result->file == NULL) {
        fprintf(stderr, "<3>unable to open file for output: %s\n", file_path);
        dsp_config_destroy(result);
        return -1;
    }

    // setup queue
    queue *client_queue = NULL;
    code = create_queue(config->core->server_config->buffer_size, config->core->server_config->queue_size,
                        &client_queue);
    if (code != 0) {
        dsp_config_destroy(result);
        return -1;
    }
    result->queue = client_queue;

    // start processing
    pthread_t dsp_thread;
    code = pthread_create(&dsp_thread, NULL, &dsp_worker, result);
    if (code != 0) {
        dsp_config_destroy(result);
        return -1;
    }
    result->dsp_thread = dsp_thread;

    pthread_mutex_lock(&config->core->mutex);
    struct sdr_config *sdr = linked_list_find(config, &core_find_closest_sdr_config, config->core->sdr_configs);
    if (sdr != NULL) {
        linked_list_add(result, &dsp_config_destroy, &sdr->dsp_configs);
    } else {
        //FIXME create and start new sdr client
    }
    pthread_mutex_unlock(&config->core->mutex);
    return code;
}

bool core_find_dsp_config(void *id, void *data) {
    uint32_t config_id = *(uint32_t *) id;
    struct dsp_config *config = (struct dsp_config *) data;
    if (config->config->id == config_id) {
        return true;
    }
    return false;
}

void core_sdr_config_destroy(void *data) {
    struct sdr_config *config = (struct sdr_config *) data;
    if (config->dsp_configs != NULL) {
        linked_list_destroy(config->dsp_configs);
    }
    pthread_join(config->sdr_thread, NULL);
    free(config);
}

bool core_sdr_config_destroy_by_id(void *id, void *data) {
    struct sdr_config *config = (struct sdr_config *) data;
    if (config->dsp_configs == NULL) {
        return false;
    }
    linked_list_destroy_by_id(id, &core_find_dsp_config, &config->dsp_configs);
    if (config->dsp_configs == NULL) {
        return true;
    }
    return false;
}

void core_remove_client(struct client_config *config) {
    if (config == NULL) {
        return;
    }
    pthread_mutex_lock(&config->core->mutex);
    uint32_t id = config->id;
    linked_list_destroy_by_id(&id, &core_sdr_config_destroy_by_id, &config->core->sdr_configs);
    pthread_mutex_unlock(&config->core->mutex);
}

void core_destroy(core *core) {
    if (core == NULL) {
        return;
    }
    pthread_mutex_lock(&core->mutex);
    linked_list_destroy(core->sdr_configs);
    pthread_mutex_unlock(&core->mutex);
    free(core);
}

