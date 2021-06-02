#include <stdlib.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>

#include "api.h"
#include "tcp_server.h"
#include "linked_list.h"
#include "api_utils.h"
#include "dsp_worker.h"
#include "sdr_worker.h"
#include "tcp_utils.h"
#include "dsp/gfsk_mod.h"
#include <math.h>
#include "dsp/doppler.h"
#include "sdr/sdr_device.h"
#include "sdr/plutosdr.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct tcp_worker {
    struct request *req;
    int client_socket;
    uint32_t id;
    atomic_bool is_running;

    pthread_t client_thread;
    tcp_server *server;
    sdr_worker *sdr;

    uint32_t buffer_size;
    uint8_t *buffer;
    gfsk_mod *fsk_mod;
    doppler *dopp;
    FILE *tx_dump_file;
    sdr_device *device;
};

struct tcp_server_t {
    int server_socket;
    volatile sig_atomic_t is_running;
    pthread_t acceptor_thread;
    struct server_config *server_config;
    uint32_t client_counter;

    linked_list *tcp_workers;
    pthread_mutex_t mutex;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
    char str[INET_ADDRSTRLEN];
    const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
    printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

int tcp_worker_convert(struct request *req, struct sdr_worker_rx **result) {
    struct sdr_worker_rx *rx = malloc(sizeof(struct sdr_worker_rx));
    if (rx == NULL) {
        return -ENOMEM;
    }
    rx->rx_sampling_freq = req->rx_sampling_freq;
    rx->rx_center_freq = req->rx_center_freq;
    rx->band_freq = req->rx_sdr_server_band_freq;

    *result = rx;
    return 0;
}

int validate_client_request(struct request *req, uint32_t client_id, struct server_config *config) {
    if (req->demod_type != REQUEST_MODEM_TYPE_NONE && req->demod_type != REQUEST_MODEM_TYPE_FSK) {
        fprintf(stderr, "<3>[%d] unknown demod_type: %d\n", client_id, req->demod_type);
        return -1;
    }
    if (req->demod_type != REQUEST_MODEM_TYPE_NONE) {
        if (req->rx_center_freq == 0) {
            fprintf(stderr, "<3>[%d] missing rx_center_freq parameter\n", client_id);
            return -1;
        }
        if (req->rx_sampling_freq == 0) {
            fprintf(stderr, "<3>[%d] missing rx_sampling_freq parameter\n", client_id);
            return -1;
        }
        if (req->rx_dump_file != REQUEST_DUMP_FILE_YES && req->rx_dump_file != REQUEST_DUMP_FILE_NO) {
            fprintf(stderr, "<3>[%d] unknown rx_dump_file: %d\n", client_id, req->rx_dump_file);
            return -1;
        }
        if (config->rx_sdr_type != RX_SDR_TYPE_SDR_SERVER) {
            fprintf(stderr, "<3>[%d] unknown rx_sdr_type: %d\n", client_id, config->rx_sdr_type);
        }
        if (config->rx_sdr_type == RX_SDR_TYPE_SDR_SERVER && req->rx_sdr_server_band_freq == 0) {
            fprintf(stderr, "<3>[%d] missing rx_sdr_server_band_freq parameter\n", client_id);
            return -1;
        }
        if (req->demod_baud_rate == 0) {
            fprintf(stderr, "<3>[%d] missing demod_baud_rate parameter\n", client_id);
            return -1;
        }
    }
    if (req->correct_doppler != REQUEST_CORRECT_DOPPLER_NO && req->correct_doppler != REQUEST_CORRECT_DOPPLER_YES) {
        fprintf(stderr, "<3>[%d] unknown correct_doppler: %d\n", client_id, req->correct_doppler);
        return -1;
    }
    if (req->demod_type == REQUEST_MODEM_TYPE_FSK) {
        if (req->demod_decimation == 0) {
            fprintf(stderr, "<3>[%d] missing demod_decimation parameter\n", client_id);
            return -1;
        }
        if (req->demod_fsk_transition_width == 0) {
            fprintf(stderr, "<3>[%d] missing demod_fsk_transition_width parameter\n", client_id);
            return -1;
        }
        if (req->demod_fsk_use_dc_block != REQUEST_DEMOD_FSK_USE_DC_BLOCK_NO && req->demod_fsk_use_dc_block != REQUEST_DEMOD_FSK_USE_DC_BLOCK_YES) {
            fprintf(stderr, "<3>[%d] unknown demod_fsk_use_dc_block: %d\n", client_id, req->demod_fsk_use_dc_block);
            return -1;
        }
        if (req->demod_destination != REQUEST_DEMOD_DESTINATION_FILE && req->demod_destination != REQUEST_DEMOD_DESTINATION_SOCKET && req->demod_destination != REQUEST_DEMOD_DESTINATION_BOTH) {
            fprintf(stderr, "<3>[%d] unknown demod_destination: %d\n", client_id, req->demod_destination);
            return -1;
        }
    }
    if (req->mod_type != REQUEST_MODEM_TYPE_NONE && req->mod_type != REQUEST_MODEM_TYPE_FSK) {
        fprintf(stderr, "<3>[%d] unknown mod_type: %d\n", client_id, req->mod_type);
        return -1;
    }
    if (req->mod_type != REQUEST_MODEM_TYPE_NONE) {
        if (req->tx_center_freq == 0) {
            fprintf(stderr, "<3>[%d] missing tx_center_freq parameter\n", client_id);
            return -1;
        }
        if (req->tx_sampling_freq == 0) {
            fprintf(stderr, "<3>[%d] missing tx_sampling_freq parameter\n", client_id);
            return -1;
        }
        if (req->tx_dump_file != REQUEST_DUMP_FILE_YES && req->tx_dump_file != REQUEST_DUMP_FILE_NO) {
            fprintf(stderr, "<3>[%d] unknown tx_dump_file: %d\n", client_id, req->tx_dump_file);
            return -1;
        }
        if (req->mod_baud_rate == 0) {
            fprintf(stderr, "<3>[%d] missing mod_baud_rate parameter\n", client_id);
            return -1;
        }
    }
    return 0;
}

int write_message(int socket, uint8_t status, uint8_t details) {
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_RESPONSE;
    struct response resp;
    resp.status = status;
    resp.details = details;

    // it is possible to directly populate *buffer with the fields,
    // however populating structs and then serializing them into byte array
    // is more readable
    size_t total_len = sizeof(struct message_header) + sizeof(struct response);
    uint8_t *buffer = malloc(total_len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    memcpy(buffer, &header, sizeof(struct message_header));
    memcpy(buffer + sizeof(struct message_header), &resp, sizeof(struct response));

    int code = tcp_utils_write_data(buffer, total_len, socket);
    free(buffer);
    return code;
}

void respond_failure(int client_socket, uint8_t status, uint8_t details) {
    write_message(client_socket, status, details); // unable to start device
    close(client_socket);
}

void handle_tx_data(struct tcp_worker *worker) {
    uint32_t len = 0;
    int code = tcp_utils_read_data(&len, sizeof(uint32_t), worker->client_socket);
    if (code != 0) {
        return;
    }
    uint32_t left = len;
    while (left > 0) {
        uint32_t batch;
        if (left < worker->buffer_size) {
            batch = left;
        } else {
            batch = worker->buffer_size;
        }
        code = tcp_utils_read_data(worker->buffer, batch, worker->client_socket);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to read tx request fully\n", worker->id);
            return;
        }

        float complex *output = NULL;
        size_t output_len = 0;
        if (worker->fsk_mod != NULL) {
            gfsk_mod_process(worker->buffer, batch, &output, &output_len, worker->fsk_mod);
        }

        if (worker->dopp != NULL) {
            float complex *dopp_output = NULL;
            size_t dopp_output_len = 0;
            doppler_process_tx(output, output_len, &dopp_output, &dopp_output_len, worker->dopp);
            output = dopp_output;
            output_len = dopp_output_len;
        }

        if (worker->tx_dump_file != NULL) {
            size_t n_written = fwrite(output, sizeof(float complex), output_len, worker->tx_dump_file);
            if (n_written < output_len) {
                fprintf(stderr, "<3>[%d] unable to write tx data\n", worker->id);
                //ignore full disk
                //continue transmitting
            }
        }

        if (worker->device != NULL) {
            code = worker->device->sdr_process_tx(output, output_len, worker->device->plugin);
            if (code != 0) {
                fprintf(stderr, "<3>[%d] unable to transmit request fully\n", worker->id);
                return;
            }
        }

        left -= batch;
    }
}

static void *tcp_worker_callback(void *arg) {
    struct tcp_worker *worker = (struct tcp_worker *) arg;
    uint32_t id = worker->id;
    fprintf(stdout, "[%d] tcp_worker is starting\n", id);
    while (worker->is_running) {
        struct message_header header;
        int code = tcp_utils_read_data(&header, sizeof(struct message_header), worker->client_socket);
        if (code < -1) {
            // read timeout happened. it's ok.
            // client already sent all information we need
            continue;
        }
        if (code == -1) {
            fprintf(stdout, "[%d] client disconnected\n", id);
            break;
        }
        if (header.protocol_version != PROTOCOL_VERSION) {
            fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", id, header.protocol_version);
            continue;
        }
        if (header.type == TYPE_SHUTDOWN) {
            fprintf(stdout, "[%d] client requested disconnect\n", id);
            break;
        } else if (header.type == TYPE_TX_DATA) {
            fprintf(stdout, "[%d] received tx data\n", id);
            handle_tx_data(worker);
        } else {
            fprintf(stderr, "<3>[%d] unsupported request: %d\n", id, header.type);
        }
    }
    //terminate dsp_worker if any
    //terminate sdr_worker if no more dsp_workers there
    sdr_worker_destroy_by_dsp_worker_id(worker->id, worker->sdr);

    worker->is_running = false;
    close(worker->client_socket);
    return (void *) 0;
}

bool tcp_worker_is_stopped(void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    if (worker->is_running) {
        return false;
    }
    return true;
}

bool tcp_worker_find_closest(void *id, void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    return sdr_worker_find_closest(id, worker->sdr);
}

void tcp_worker_destroy(void *data) {
    if (data == NULL) {
        return;
    }
    struct tcp_worker *worker = (struct tcp_worker *) data;
    fprintf(stdout, "[%d] tcp_worker is stopping\n", worker->id);
    worker->is_running = false;
    close(worker->client_socket);
    pthread_join(worker->client_thread, NULL);
    if (worker->req != NULL) {
        free(worker->req);
    }
    if (worker->buffer != NULL) {
        free(worker->buffer);
    }
    if (worker->fsk_mod != NULL) {
        gfsk_mod_destroy(worker->fsk_mod);
    }
    if (worker->dopp != NULL) {
        doppler_destroy(worker->dopp);
    }
    if (worker->tx_dump_file != NULL) {
        fclose(worker->tx_dump_file);
    }
    if (worker->device != NULL) {
        worker->device->destroy(worker->device->plugin);
        free(worker->device);
    }
    uint32_t id = worker->id;
    free(worker);
    fprintf(stdout, "[%d] tcp_worker stopped\n", id);
}

void cleanup_terminated_threads(tcp_server *server) {
    pthread_mutex_lock(&server->mutex);
    linked_list_destroy_by_selector(&tcp_worker_is_stopped, &server->tcp_workers);
    pthread_mutex_unlock(&server->mutex);
}

void handle_new_client(int client_socket, tcp_server *server) {
    struct tcp_worker *tcp_worker = malloc(sizeof(struct tcp_worker));
    if (tcp_worker == NULL) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        return;
    }
    *tcp_worker = (struct tcp_worker) {0};
    tcp_worker->id = server->client_counter;

    struct request *req = malloc(sizeof(struct request));
    if (req == NULL) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }
    *req = (struct request) {0};
    tcp_worker->req = req;

    tcp_worker->buffer_size = server->server_config->buffer_size;
    tcp_worker->buffer = malloc(sizeof(uint8_t) * tcp_worker->buffer_size);
    if (tcp_worker->buffer == NULL) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    if (tcp_utils_read_data(tcp_worker->req, sizeof(struct request), client_socket) < 0) {
        fprintf(stderr, "<3>[%d] unable to read request fully\n", tcp_worker->id);
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }
    api_network_to_host(tcp_worker->req);

    if (validate_client_request(tcp_worker->req, tcp_worker->id, server->server_config) < 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    struct sdr_worker_rx *rx = NULL;
    int code = tcp_worker_convert(tcp_worker->req, &rx);
    if (code != 0) {
        fprintf(stderr, "<3>[%d] unable to create rx\n", tcp_worker->id);
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }
    if (tcp_worker->req->mod_type == REQUEST_MODEM_TYPE_FSK) {
        code = gfsk_mod_create((float) tcp_worker->req->tx_sampling_freq / tcp_worker->req->mod_baud_rate, (2 * M_PI * tcp_worker->req->mod_fsk_deviation / tcp_worker->req->tx_sampling_freq), 0.5F, tcp_worker->buffer_size, &tcp_worker->fsk_mod);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create fsk modulator\n", tcp_worker->id);
            respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            free(rx);
            return;
        }
    }
    if (tcp_worker->req->mod_type != REQUEST_MODEM_TYPE_NONE) {
        if (req->correct_doppler == REQUEST_CORRECT_DOPPLER_YES) {
            int samples_per_symbol = (int) ((float) tcp_worker->req->tx_sampling_freq / tcp_worker->req->mod_baud_rate);
            code = doppler_create(req->latitude / 10E6F, req->longitude / 10E6F, req->altitude / 10E3F, req->tx_sampling_freq, req->tx_center_freq, 0, samples_per_symbol * server->server_config->buffer_size, req->tle, &tcp_worker->dopp);
            if (code != 0) {
                fprintf(stderr, "<3>[%d] unable to create tx doppler correction block\n", tcp_worker->id);
                respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
                dsp_worker_destroy(tcp_worker);
                free(rx);
                return;
            }
        }
        if (req->rx_dump_file == REQUEST_DUMP_FILE_YES) {
            char file_path[4096];
            snprintf(file_path, sizeof(file_path), "%s/tx.mod2sdr.%d.cf32", server->server_config->base_path, tcp_worker->id);
            tcp_worker->tx_dump_file = fopen(file_path, "wb");
            if (tcp_worker->tx_dump_file == NULL) {
                fprintf(stderr, "<3>[%d] unable to open file for tx output: %s\n", tcp_worker->id, file_path);
                respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
                dsp_worker_destroy(tcp_worker);
                free(rx);
                return;
            }
        }
        if (server->server_config->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
            struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
            if (tx_config == NULL) {
                fprintf(stderr, "<3>[%d] unable to init tx configuration\n", tcp_worker->id);
                respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
                dsp_worker_destroy(tcp_worker);
                free(rx);
                return;
            }
            tx_config->sampling_freq = tcp_worker->req->tx_sampling_freq;
            tx_config->center_freq = tcp_worker->req->tx_center_freq;
            tx_config->manual_gain = server->server_config->tx_plutosdr_gain;
            code = plutosdr_create(tcp_worker->id, NULL, tx_config, server->server_config->tx_plutosdr_timeout_millis, server->server_config->buffer_size, server->server_config->iio, &tcp_worker->device);
            if (code != 0) {
                fprintf(stderr, "<3>[%d] unable to init pluto tx\n", tcp_worker->id);
                respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
                dsp_worker_destroy(tcp_worker);
                free(rx);
                return;
            }
        }
    }
    tcp_worker->is_running = true;
    tcp_worker->client_socket = client_socket;
    tcp_worker->server = server;

    cleanup_terminated_threads(server);

    pthread_t client_thread;
    code = pthread_create(&client_thread, NULL, &tcp_worker_callback, tcp_worker);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        free(rx);
        return;
    }
    tcp_worker->client_thread = client_thread;

    dsp_worker *dsp_worker = NULL;
    code = dsp_worker_create(tcp_worker->id, tcp_worker->client_socket, server->server_config, tcp_worker->req, &dsp_worker);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        free(rx);
        return;
    }

    pthread_mutex_lock(&server->mutex);
    //re-use sdr connections
    //this will allow demodulating different modes using the same data
    struct tcp_worker *closest = linked_list_find(rx, &tcp_worker_find_closest, server->tcp_workers);
    if (closest == NULL) {
        sdr_worker *sdr = NULL;
        // take id from the first tcp client
        code = sdr_worker_create(tcp_worker->id, rx, server->server_config->rx_sdr_server_address, server->server_config->rx_sdr_server_port, server->server_config->read_timeout_seconds, server->server_config->buffer_size, &sdr);
        if (code != 0) {
            respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            dsp_worker_destroy(dsp_worker);
            pthread_mutex_unlock(&server->mutex);
            return;
        }
        tcp_worker->sdr = sdr;
    } else {
        tcp_worker->sdr = closest->sdr;
        free(rx);
    }

    code = sdr_worker_add_dsp_worker(dsp_worker, tcp_worker->sdr);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        // this will trigger sdr_worker destroy
        tcp_worker_destroy(tcp_worker);
        dsp_worker_destroy(dsp_worker);
        pthread_mutex_unlock(&server->mutex);
        return;
    }

    code = linked_list_add(tcp_worker, &tcp_worker_destroy, &server->tcp_workers);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        // this will trigger sdr_worker destroy
        tcp_worker_destroy(tcp_worker);
        dsp_worker_destroy(dsp_worker);
        pthread_mutex_unlock(&server->mutex);
        return;
    }
    pthread_mutex_unlock(&server->mutex);

    write_message(tcp_worker->client_socket, RESPONSE_STATUS_SUCCESS, tcp_worker->id);
    fprintf(stdout, "[%d] demod %s rx center_freq %d rx sampling_rate %d demod destination %d\n", tcp_worker->id,
            api_demod_type_str(tcp_worker->req->demod_type), tcp_worker->req->rx_center_freq,
            tcp_worker->req->rx_sampling_freq, tcp_worker->req->demod_destination);

}

static void *acceptor_worker(void *arg) {
    tcp_server *server = (tcp_server *) arg;
    struct sockaddr_in address;
    while (server->is_running) {
        int client_socket;
        int addrlen = sizeof(address);
        if ((client_socket = accept(server->server_socket, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            break;
        }

        struct timeval tv;
        tv.tv_sec = server->server_config->read_timeout_seconds;
        tv.tv_usec = 0;
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv)) {
            close(client_socket);
            perror("setsockopt - SO_RCVTIMEO");
            continue;
        }

        // always increment counter to make even error messages traceable
        server->client_counter++;

        struct message_header header;
        if (tcp_utils_read_data(&header, sizeof(struct message_header), client_socket) < 0) {
            fprintf(stderr, "<3>[%d] unable to read request header fully\n", server->client_counter);
            respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
            continue;
        }
        if (header.protocol_version != PROTOCOL_VERSION) {
            fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", server->client_counter, header.protocol_version);
            respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
            continue;
        }

        switch (header.type) {
            case TYPE_REQUEST:
                log_client(&address, server->client_counter);
                handle_new_client(client_socket, server);
                break;
            case TYPE_PING:
                write_message(client_socket, RESPONSE_STATUS_SUCCESS, 0);
                close(client_socket);
                break;
            default:
                fprintf(stderr, "<3>[%d] unsupported request: %d\n", server->client_counter, header.type);
                respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
                break;
        }

    }

    linked_list_destroy(server->tcp_workers);
    server->tcp_workers = NULL;

    printf("tcp server stopped\n");
    return (void *) 0;
}

int tcp_server_create(struct server_config *config, tcp_server **server) {
    tcp_server *result = malloc(sizeof(struct tcp_server_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct tcp_server_t) {0};
    result->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    result->tcp_workers = NULL;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        free(result);
        perror("socket creation failed");
        return -1;
    }
    result->server_socket = server_socket;
    result->is_running = true;
    result->server_config = config;
    // start counting from 0
    result->client_counter = -1;
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        free(result);
        perror("setsockopt - SO_REUSEADDR");
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
        free(result);
        perror("setsockopt - SO_REUSEPORT");
        return -1;
    }
#endif

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, config->bind_address, &address.sin_addr) <= 0) {
        free(result);
        perror("invalid address");
        return -1;
    }
    address.sin_port = htons(config->port);

    if (bind(server_socket, (struct sockaddr *) &address, sizeof(address)) < 0) {
        free(result);
        perror("bind failed");
        return -1;
    }
    if (listen(server_socket, 3) < 0) {
        free(result);
        perror("listen failed");
        return -1;
    }

    pthread_t acceptor_thread;
    int code = pthread_create(&acceptor_thread, NULL, &acceptor_worker, result);
    if (code != 0) {
        free(result);
        return -1;
    }
    result->acceptor_thread = acceptor_thread;

    *server = result;
    return 0;
}

void tcp_server_join_thread(tcp_server *server) {
    pthread_join(server->acceptor_thread, NULL);
}

void tcp_server_destroy(tcp_server *server) {
    if (server == NULL) {
        return;
    }
    fprintf(stdout, "tcp server is stopping\n");
    server->is_running = false;
    // close is not enough to exit from the blocking "accept" method
    // execute shutdown first
    int code = shutdown(server->server_socket, SHUT_RDWR);
    if (code != 0) {
        close(server->server_socket);
    }
    pthread_join(server->acceptor_thread, NULL);

    free(server);
}
