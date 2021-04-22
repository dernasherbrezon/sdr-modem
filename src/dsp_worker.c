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
#include <unistd.h>

struct dsp_worker_t {
    uint8_t rx_destination;
    uint32_t id;
    int client_socket;
    fsk_demod *fsk_demod;
    doppler *dopp;
    queue *queue;
    pthread_t dsp_thread;
    FILE *file;
};

bool dsp_worker_find_by_id(void *id, void *data) {
    uint32_t config_id = *(uint32_t *) id;
    dsp_worker *worker = (dsp_worker *) data;
    if (worker->id == config_id) {
        return true;
    }
    return false;
}

int write_to_file(dsp_worker *worker, int8_t *filter_output, size_t filter_output_len) {
    size_t n_written;
    if (worker->file != NULL) {
        n_written = fwrite(filter_output, sizeof(int8_t), filter_output_len, worker->file);
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

int write_to_socket(dsp_worker *worker, int8_t *filter_output, size_t filter_output_len) {
    size_t total_len = filter_output_len * sizeof(int8_t);
    size_t left = total_len;
    while (left > 0) {
        int written = write(worker->client_socket, (char *) filter_output + (total_len - left), left);
        if (written < 0) {
            return -1;
        }
        left -= written;
    }
    return 0;
}

static void *dsp_worker_callback(void *arg) {
    dsp_worker *worker = (dsp_worker *) arg;
    fprintf(stdout, "[%d] dsp_worker is starting\n", worker->id);
    float complex *input = NULL;
    size_t input_len = 0;
    int8_t *demod_output = NULL;
    size_t demod_output_len = 0;
    while (true) {
        take_buffer_for_processing(&input, &input_len, worker->queue);
        // poison pill received
        if (input == NULL) {
            break;
        }
        if (worker->dopp != NULL) {
            float complex *doppler_output = NULL;
            size_t doppler_output_len = 0;
            doppler_process(input, input_len, &doppler_output, &doppler_output_len, worker->dopp);
            input = doppler_output;
            input_len = doppler_output_len;
        }
        if (worker->fsk_demod != NULL) {
            fsk_demod_process(input, input_len, &demod_output, &demod_output_len, worker->fsk_demod);
        }
        if (demod_output == NULL) {
            // shouldn't happen
            // demod type checked on client request
            continue;
        }
        int code;
        if (worker->rx_destination == REQUEST_RX_DESTINATION_FILE) {
            code = write_to_file(worker, demod_output, demod_output_len);
        } else if (worker->rx_destination == REQUEST_RX_DESTINATION_SOCKET) {
            code = write_to_socket(worker, demod_output, demod_output_len);
        } else {
            fprintf(stderr, "<3>unknown destination: %d\n", worker->rx_destination);
            code = -1;
        }

        complete_buffer_processing(worker->queue);

        if (code != 0) {
            // this would trigger client disconnect
            // I could use "break" here, but "continue" is a bit better:
            //   - single route for abnormal termination (i.e. disk space issue) and normal (i.e. client disconnected)
            //   - all shutdown sequences have: stop tcp thread, then dsp thread, then sdr thread
            //   - processing the queue and writing to the already full disk is OK (I hope)
            //   - calling "close" socket multiple times is OK (I hope)
            close(worker->client_socket);
        }

    }
    destroy_queue(worker->queue);
    printf("[%d] dsp_worker stopped\n", worker->id);
    return (void *) 0;
}

int dsp_worker_create(uint32_t id, int client_socket, struct server_config *server_config, struct request *req,
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
    if (req->correct_doppler == REQUEST_CORRECT_DOPPLER_YES) {
        code = doppler_create(req->latitude / 10E6F, req->longitude / 10E6F, req->altitude / 10E3F, req->rx_sampling_freq, req->rx_center_freq, 0, server_config->buffer_size, req->tle, &result->dopp);
    }
    if (code != 0) {
        fprintf(stderr, "<3>unable to create doppler correction block\n");
        dsp_worker_destroy(result);
        return code;
    }

    if (req->demod_type == REQUEST_DEMOD_TYPE_FSK) {
        code = fsk_demod_create(req->rx_sampling_freq, req->demod_baud_rate,
                                req->demod_fsk_deviation, req->demod_decimation,
                                req->demod_fsk_transition_width, req->demod_fsk_use_dc_block,
                                server_config->buffer_size, &result->fsk_demod);
    }

    if (code != 0) {
        fprintf(stderr, "<3>unable to create demodulator\n");
        dsp_worker_destroy(result);
        return code;
    }

    if (req->rx_destination == REQUEST_RX_DESTINATION_FILE) {
        char file_path[4096];
        snprintf(file_path, sizeof(file_path), "%s/%d.cf32", server_config->base_path, id);
        result->file = fopen(file_path, "wb");
        if (result->file == NULL) {
            fprintf(stderr, "<3>unable to open file for output: %s\n", file_path);
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
    // cleanup everything only when thread terminates
    if (worker->file != NULL) {
        fclose(worker->file);
    }
    if (worker->fsk_demod != NULL) {
        fsk_demod_destroy(worker->fsk_demod);
    }
    if (worker->dopp != NULL) {
        doppler_destroy(worker->dopp);
    }
    free(worker);
}


