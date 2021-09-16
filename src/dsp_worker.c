#include "dsp_worker.h"
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include "dsp/fsk_demod.h"
#include "queue.h"
#include "api.h"
#include "dsp/doppler.h"
#include <string.h>
#include <complex.h>
#include "tcp_utils.h"
#include "api_utils.h"

struct dsp_worker_t {
    uint32_t id;
    int client_socket;
    fsk_demod *fsk_demod;
    doppler *dopp;
    queue *queue;
    pthread_t dsp_thread;
    FILE *rx_dump_file;
    FILE *demod_file;
    uint8_t demod_destination;
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
        if (worker->rx_dump_file != NULL) {
            size_t n_written = fwrite(input, sizeof(float complex), input_len, worker->rx_dump_file);
            // if disk is full, then terminate the client
            if (n_written < input_len) {
                complete_buffer_processing(worker->queue);
                fprintf(stderr, "<3>[%d] unable to write sdr data\n", id);
                break;
            }
        }
        if (worker->dopp != NULL) {
            float complex *doppler_output = NULL;
            size_t doppler_output_len = 0;
            doppler_process_rx(input, input_len, &doppler_output, &doppler_output_len, worker->dopp);
            input = doppler_output;
            input_len = doppler_output_len;
        }
        int8_t *demod_output = NULL;
        size_t demod_output_len = 0;
        if (worker->fsk_demod != NULL) {
            fsk_demod_process(input, input_len, &demod_output, &demod_output_len, worker->fsk_demod);
        }
        if (demod_output == NULL) {
            complete_buffer_processing(worker->queue);
            // can be NULL if demod_type = NONE
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
        int code = 0;
        if (worker->demod_destination == DEMOD_DESTINATION__SOCKET || worker->demod_destination == DEMOD_DESTINATION__BOTH) {
            code = tcp_utils_write_data((uint8_t *) demod_output, demod_output_len, worker->client_socket);
        }

        complete_buffer_processing(worker->queue);

        if (code != 0) {
            break;
        }

    }
    printf("[%d] dsp_worker stopped\n", worker->id);
    return (void *) 0;
}

int dsp_worker_create(uint32_t id, int client_socket, struct server_config *server_config, struct RxRequest *req,
                      dsp_worker **worker) {
    struct dsp_worker_t *result = malloc(sizeof(struct dsp_worker_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct dsp_worker_t) {0};
    result->id = id;
    result->client_socket = client_socket;

    int code = 0;
    if (req->doppler != NULL) {
        struct DopplerSettings *doppler_settings = req->doppler;
        char tle[3][80];
        api_utils_convert_tle(doppler_settings->tle, tle);
        code = doppler_create(doppler_settings->latitude / 10E6, doppler_settings->longitude / 10E6, doppler_settings->altitude / 10E3, req->rx_sampling_freq, req->rx_center_freq, 0, 0, server_config->buffer_size, tle, &result->dopp);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create doppler correction block\n", result->id);
            dsp_worker_destroy(result);
            return code;
        }
    }

    if (req->demod_type == MODEM_TYPE__GMSK) {
        struct FskDemodulationSettings *fsk_settings = req->fsk_settings;
        code = fsk_demod_create(req->rx_sampling_freq, req->demod_baud_rate,
                                fsk_settings->demod_fsk_deviation, req->demod_decimation,
                                fsk_settings->demod_fsk_transition_width, fsk_settings->demod_fsk_use_dc_block,
                                server_config->buffer_size, &result->fsk_demod);
    }

    if (code != 0) {
        fprintf(stderr, "<3>[%d] unable to create demodulator\n", result->id);
        dsp_worker_destroy(result);
        return code;
    }

    if (req->rx_dump_file) {
        char file_path[4096];
        snprintf(file_path, sizeof(file_path), "%s/rx.sdr2demod.%d.cf32", server_config->base_path, id);
        result->rx_dump_file = fopen(file_path, "wb");
        if (result->rx_dump_file == NULL) {
            fprintf(stderr, "<3>[%d] unable to open file for sdr input: %s\n", result->id, file_path);
            dsp_worker_destroy(result);
            return -1;
        }
    }
    result->demod_destination = req->demod_destination;
    if (req->demod_destination == DEMOD_DESTINATION__FILE || req->demod_destination == DEMOD_DESTINATION__BOTH) {
        char file_path[4096];
        snprintf(file_path, sizeof(file_path), "%s/rx.demod2client.%d.s8", server_config->base_path, id);
        result->demod_file = fopen(file_path, "wb");
        if (result->demod_file == NULL) {
            fprintf(stderr, "<3>[%d] unable to open file for demod output: %s\n", result->id, file_path);
            dsp_worker_destroy(result);
            return -1;
        }
    }

    // setup queue
    queue *client_queue = NULL;
    code = create_queue(server_config->buffer_size, server_config->queue_size,
                        &client_queue);
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
    // wait until thread terminates and only then destroy the worker
    pthread_join(worker->dsp_thread, NULL);
    if (worker->queue != NULL) {
        destroy_queue(worker->queue);
    }
    // cleanup everything only when thread terminates
    if (worker->rx_dump_file != NULL) {
        fclose(worker->rx_dump_file);
    }
    if (worker->demod_file != NULL) {
        fclose(worker->demod_file);
    }
    if (worker->fsk_demod != NULL) {
        fsk_demod_destroy(worker->fsk_demod);
    }
    if (worker->dopp != NULL) {
        doppler_destroy(worker->dopp);
    }
    free(worker);
}


