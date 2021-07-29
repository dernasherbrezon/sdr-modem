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
#include "api.pb-c.h"
#include "tcp_server.h"
#include "linked_list.h"
#include "api_utils.h"
#include "dsp_worker.h"
#include "sdr_worker.h"
#include "dsp/gfsk_mod.h"
#include <math.h>
#include "dsp/doppler.h"
#include "dsp/sig_source.h"
#include "sdr/sdr_device.h"
#include "sdr/plutosdr.h"
#include "sdr/sdr_server_client.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct tcp_worker {
    struct RxRequest *rx_req;
    struct TxRequest *tx_req;
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
    sig_source *signal;

    sdr_device *tx_device;
};

struct tcp_server_t {
    int server_socket;
    volatile sig_atomic_t is_running;
    pthread_t acceptor_thread;
    struct server_config *server_config;
    uint32_t client_counter;

    linked_list *tcp_workers;
    pthread_mutex_t mutex;

    bool tx_initialized;
    bool rx_initialized;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
    char str[INET_ADDRSTRLEN];
    const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
    printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

int tcp_worker_convert(struct RxRequest *req, struct sdr_rx **result) {
    struct sdr_rx *rx = malloc(sizeof(struct sdr_rx));
    if (rx == NULL) {
        return -ENOMEM;
    }
    rx->rx_sampling_freq = req->rx_sampling_freq;
    rx->rx_center_freq = req->rx_center_freq;
    rx->rx_offset = req->rx_offset;

    *result = rx;
    return 0;
}

int validate_tx_request(struct TxRequest *req, uint32_t client_id, struct server_config *config) {
    if (req->mod_type != MODEM_TYPE__GMSK) {
        fprintf(stderr, "<3>[%d] unknown mod_type: %d\n", client_id, req->mod_type);
        return -1;
    }
    if (config->tx_sdr_type == TX_SDR_TYPE_NONE) {
        fprintf(stderr, "<3>[%d] server doesn't support tx\n", client_id);
        return -1;
    }
    if (req->tx_center_freq == 0) {
        fprintf(stderr, "<3>[%d] missing tx_center_freq parameter\n", client_id);
        return -1;
    }
    if (req->tx_sampling_freq == 0) {
        fprintf(stderr, "<3>[%d] missing tx_sampling_freq parameter\n", client_id);
        return -1;
    }
    if (req->mod_baud_rate == 0) {
        fprintf(stderr, "<3>[%d] missing mod_baud_rate parameter\n", client_id);
        return -1;
    }
    if (req->doppler != NULL) {
        if (req->doppler->n_tle != 3) {
            fprintf(stderr, "<3>[%d] invalid tle supplied\n", client_id);
            return -1;
        }
    }
    return 0;
}

int validate_rx_request(struct RxRequest *req, uint32_t client_id, struct server_config *config) {
    if (req->demod_type != MODEM_TYPE__GMSK) {
        fprintf(stderr, "<3>[%d] unknown demod_type: %d\n", client_id, req->demod_type);
        return -1;
    }
    if (req->rx_center_freq == 0) {
        fprintf(stderr, "<3>[%d] missing rx_center_freq parameter\n", client_id);
        return -1;
    }
    if (req->rx_sampling_freq == 0) {
        fprintf(stderr, "<3>[%d] missing rx_sampling_freq parameter\n", client_id);
        return -1;
    }
    if (req->demod_baud_rate == 0) {
        fprintf(stderr, "<3>[%d] missing demod_baud_rate parameter\n", client_id);
        return -1;
    }
    if (req->doppler != NULL) {
        if (req->doppler->n_tle != 3) {
            fprintf(stderr, "<3>[%d] invalid tle supplied\n", client_id);
            return -1;
        }
    }
    if (req->demod_decimation == 0) {
        fprintf(stderr, "<3>[%d] missing demod_decimation parameter\n", client_id);
        return -1;
    }
    if (req->demod_destination != DEMOD_DESTINATION__FILE && req->demod_destination != DEMOD_DESTINATION__SOCKET && req->demod_destination != DEMOD_DESTINATION__BOTH) {
        fprintf(stderr, "<3>[%d] unknown demod_destination: %d\n", client_id, req->demod_destination);
        return -1;
    }
    if (req->demod_type == MODEM_TYPE__GMSK) {
        if (req->fsk_settings == NULL) {
            fprintf(stderr, "<3>[%d] missing fsk_settings parameter\n", client_id);
            return -1;
        }
        if (req->fsk_settings->demod_fsk_transition_width == 0) {
            fprintf(stderr, "<3>[%d] missing demod_fsk_transition_width parameter\n", client_id);
            return -1;
        }
    }
    return 0;
}

void tcp_server_write_response_and_close(int client_socket, ResponseStatus status, uint32_t details) {
    api_utils_write_response(client_socket, status, details);
    close(client_socket);
}

void handle_tx_data(struct tcp_worker *worker, struct message_header *header) {
    TxData *data = NULL;
    int code = api_utils_read_tx_data(worker->client_socket, header, &data);
    if (code != 0) {
        fprintf(stderr, "<3>[%d] unable to read tx request fully\n", worker->id);
        api_utils_write_response(worker->client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        return;
    }
    uint32_t left = data->data.len;
    uint32_t processed = 0;
    while (left > 0) {
        uint32_t batch;
        if (left < worker->buffer_size) {
            batch = left;
        } else {
            batch = worker->buffer_size;
        }
        float complex *output = NULL;
        size_t output_len = 0;
        if (worker->fsk_mod != NULL) {
            gfsk_mod_process(data->data.data + processed, batch, &output, &output_len, worker->fsk_mod);
        }

        if (worker->dopp != NULL) {
            float complex *dopp_output = NULL;
            size_t dopp_output_len = 0;
            doppler_process_tx(output, output_len, &dopp_output, &dopp_output_len, worker->dopp);
            output = dopp_output;
            output_len = dopp_output_len;
        }
        if (worker->signal != NULL) {
            float complex *signal_output = NULL;
            size_t signal_output_len = 0;
            sig_source_multiply(worker->tx_req->tx_offset, output, output_len, &signal_output, &signal_output_len, worker->signal);
            output = signal_output;
            output_len = signal_output_len;
        }

        if (worker->tx_dump_file != NULL) {
            size_t n_written = fwrite(output, sizeof(float complex), output_len, worker->tx_dump_file);
            if (n_written < output_len) {
                fprintf(stderr, "<3>[%d] unable to write tx data\n", worker->id);
                //ignore full disk
                //continue transmitting
            }
        }

        if (worker->tx_device != NULL) {
            code = worker->tx_device->sdr_process_tx(output, output_len, worker->tx_device->plugin);
            if (code != 0) {
                fprintf(stderr, "<3>[%d] unable to transmit request fully\n", worker->id);
                api_utils_write_response(worker->client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
                break;
            }
        }

        left -= batch;
        processed += batch;
    }

    if (left == 0) {
        fprintf(stdout, "[%d] successfully sent %zu bytes\n", worker->id, data->data.len);
        api_utils_write_response(worker->client_socket, RESPONSE_STATUS__SUCCESS, RESPONSE_NO_DETAILS);
    }
    tx_data__free_unpacked(data, NULL);
}

static void *tcp_worker_callback(void *arg) {
    struct tcp_worker *worker = (struct tcp_worker *) arg;
    uint32_t id = worker->id;
    fprintf(stdout, "[%d] tcp_worker is starting\n", id);
    while (worker->is_running) {
        struct message_header header;
        int code = api_utils_read_header(worker->client_socket, &header);
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
            fprintf(stdout, "[%d] received tx request\n", id);
            handle_tx_data(worker, &header);
        } else {
            fprintf(stderr, "<3>[%d] unsupported request: %d\n", id, header.type);
        }
    }
    //terminate dsp_worker if any
    //terminate sdr_worker if no more dsp_workers there
    sdr_worker_destroy_by_dsp_worker_id(worker->id, worker->sdr);

    close(worker->client_socket);

    worker->is_running = false;
    return (void *) 0;
}

bool tcp_worker_is_stopped(void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    if (worker->is_running) {
        return false;
    }
    return true;
}

bool tcp_worker_is_txing(void *id, void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    return (worker->tx_device != NULL);
}

bool tcp_worker_is_rxing(void *id, void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    return (worker->rx_req != NULL);
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
    pthread_join(worker->client_thread, NULL);
    if (worker->rx_req != NULL) {
        rx_request__free_unpacked(worker->rx_req, NULL);
    }
    if (worker->tx_req != NULL) {
        tx_request__free_unpacked(worker->tx_req, NULL);
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
    if (worker->signal != NULL) {
        sig_source_destroy(worker->signal);
    }
    if (worker->tx_dump_file != NULL) {
        fclose(worker->tx_dump_file);
    }
    if (worker->tx_device != NULL) {
        worker->tx_device->destroy(worker->tx_device->plugin);
        free(worker->tx_device);
    }
    uint32_t id = worker->id;
    free(worker);
    fprintf(stdout, "[%d] tcp_worker stopped\n", id);
}

void cleanup_terminated_threads(tcp_server *server) {
    pthread_mutex_lock(&server->mutex);
    linked_list_destroy_by_selector(&tcp_worker_is_stopped, &server->tcp_workers);
    void *tx_worker = linked_list_find(NULL, &tcp_worker_is_txing, server->tcp_workers);
    if (tx_worker == NULL) {
        server->tx_initialized = false;
    }
    void *rx_worker = linked_list_find(NULL, &tcp_worker_is_rxing, server->tcp_workers);
    if (rx_worker == NULL) {
        server->rx_initialized = false;
    }
    pthread_mutex_unlock(&server->mutex);
}

int tcp_server_init_tx_device(uint32_t id, struct TxRequest *req, tcp_server *server, sdr_device **output) {
    if (server->tx_initialized) {
        fprintf(stderr, "<3>[%d] tx is being used\n", id);
        return -RESPONSE_DETAILS_TX_IS_BEING_USED;
    }
    if (server->server_config->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
        struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
        if (tx_config == NULL) {
            fprintf(stderr, "<3>[%d] unable to init tx configuration\n", id);
            return -RESPONSE_DETAILS_INTERNAL_ERROR;
        }
        tx_config->sampling_freq = req->tx_sampling_freq;
        tx_config->center_freq = req->tx_center_freq;
        tx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
        tx_config->manual_gain = server->server_config->tx_plutosdr_gain;
        int code = plutosdr_create(id, false, NULL, tx_config, server->server_config->tx_plutosdr_timeout_millis, server->server_config->buffer_size, server->server_config->iio, output);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to init pluto tx\n", id);
            return -RESPONSE_DETAILS_INTERNAL_ERROR;
        }
    } else {
        fprintf(stderr, "<3>[%d] unknown tx sdr %d\n", id, server->server_config->tx_sdr_type);
        return -RESPONSE_DETAILS_INTERNAL_ERROR;
    }
    server->tx_initialized = true;
    return 0;
}

int tcp_server_init_rx_device(dsp_worker *dsp_worker, tcp_server *server, struct tcp_worker *tcp_worker) {
    struct sdr_rx *rx = NULL;
    int code = tcp_worker_convert(tcp_worker->rx_req, &rx);
    if (code != 0) {
        return -RESPONSE_DETAILS_INTERNAL_ERROR;
    }
    if (server->server_config->rx_sdr_type == RX_SDR_TYPE_SDR_SERVER) {
        //re-use sdr connections
        //this will allow demodulating different modes using the same data
        struct tcp_worker *closest = linked_list_find(rx, &tcp_worker_find_closest, server->tcp_workers);
        if (closest == NULL) {
            sdr_device *rx_device = NULL;
            code = sdr_server_client_create(tcp_worker->id, rx, server->server_config->rx_sdr_server_address, server->server_config->rx_sdr_server_port, server->server_config->read_timeout_seconds, server->server_config->buffer_size, &rx_device);
            if (code != 0) {
                free(rx);
                return -RESPONSE_DETAILS_INTERNAL_ERROR;
            }
            sdr_worker *sdr = NULL;
            // take id from the first tcp client
            code = sdr_worker_create(tcp_worker->id, rx, rx_device, &sdr);
            if (code != 0) {
                return -RESPONSE_DETAILS_INTERNAL_ERROR;
            }
            tcp_worker->sdr = sdr;
        } else {
            tcp_worker->sdr = closest->sdr;
            free(rx);
        }
    } else if (server->server_config->rx_sdr_type == RX_SDR_TYPE_PLUTOSDR) {
        if (server->rx_initialized) {
            free(rx);
            fprintf(stderr, "<3>[%d] rx is being used\n", tcp_worker->id);
            return -RESPONSE_DETAILS_RX_IS_BEING_USED;
        }
        struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
        if (rx_config == NULL) {
            free(rx);
            fprintf(stderr, "<3>[%d] unable to init tx configuration\n", tcp_worker->id);
            return -RESPONSE_DETAILS_INTERNAL_ERROR;
        }
        rx_config->sampling_freq = tcp_worker->rx_req->rx_sampling_freq;
        rx_config->center_freq = tcp_worker->rx_req->rx_center_freq;
        rx_config->gain_control_mode = IIO_GAIN_MODE_MANUAL;
        rx_config->manual_gain = server->server_config->rx_plutosdr_gain;
        rx_config->offset = tcp_worker->rx_req->rx_offset;
        sdr_device *rx_device = NULL;
        code = plutosdr_create(tcp_worker->id, !server->tx_initialized, rx_config, NULL, server->server_config->tx_plutosdr_timeout_millis, server->server_config->buffer_size, server->server_config->iio, &rx_device);
        if (code != 0) {
            free(rx);
            fprintf(stderr, "<3>[%d] unable to init pluto rx\n", tcp_worker->id);
            return -RESPONSE_DETAILS_INTERNAL_ERROR;
        }
        sdr_worker *sdr = NULL;
        code = sdr_worker_create(tcp_worker->id, rx, rx_device, &sdr);
        if (code != 0) {
            return -RESPONSE_DETAILS_INTERNAL_ERROR;
        }
        tcp_worker->sdr = sdr;
    } else {
        free(rx);
        return -1;
    }

    code = sdr_worker_add_dsp_worker(dsp_worker, tcp_worker->sdr);
    if (code != 0) {
        return -RESPONSE_DETAILS_INTERNAL_ERROR;
    }

    code = linked_list_add(tcp_worker, &tcp_worker_destroy, &server->tcp_workers);
    if (code != 0) {
        return -RESPONSE_DETAILS_INTERNAL_ERROR;
    }

    server->rx_initialized = true;
    return 0;
}

void handle_tx_client(int client_socket, struct message_header *header, tcp_server *server) {
    struct tcp_worker *tcp_worker = malloc(sizeof(struct tcp_worker));
    if (tcp_worker == NULL) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        return;
    }
    *tcp_worker = (struct tcp_worker) {0};
    tcp_worker->id = server->client_counter;
    tcp_worker->client_socket = client_socket;
    tcp_worker->server = server;
    //explicitly init all rx fields to NULL for tx client
    tcp_worker->rx_req = NULL;
    tcp_worker->sdr = NULL;

    tcp_worker->buffer_size = server->server_config->buffer_size;
    tcp_worker->buffer = malloc(sizeof(uint8_t) * tcp_worker->buffer_size);
    if (tcp_worker->buffer == NULL) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    if (api_utils_read_tx_request(client_socket, header, &tcp_worker->tx_req) != 0) {
        fprintf(stderr, "<3>[%d] unable to read request fully\n", tcp_worker->id);
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    if (validate_tx_request(tcp_worker->tx_req, tcp_worker->id, server->server_config) < 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    int code;
    if (tcp_worker->tx_req->mod_type == MODEM_TYPE__GMSK) {
        struct FskModulationSettings *fsk_settings = tcp_worker->tx_req->fsk_settings;
        code = gfsk_mod_create((float) tcp_worker->tx_req->tx_sampling_freq / tcp_worker->tx_req->mod_baud_rate, (2 * M_PI * fsk_settings->mod_fsk_deviation / tcp_worker->tx_req->tx_sampling_freq), 0.5F, tcp_worker->buffer_size, &tcp_worker->fsk_mod);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create fsk modulator\n", tcp_worker->id);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            return;
        }
    }
    int samples_per_symbol = (int) ((float) tcp_worker->tx_req->tx_sampling_freq / tcp_worker->tx_req->mod_baud_rate);
    uint32_t max_output_buffer = samples_per_symbol * server->server_config->buffer_size;
    if (tcp_worker->tx_req->doppler != NULL) {
        struct DopplerSettings *doppler_settings = tcp_worker->tx_req->doppler;
        char tle[3][80];
        api_utils_convert_tle(doppler_settings->tle, tle);
        code = doppler_create(doppler_settings->latitude / 10E6F, doppler_settings->longitude / 10E6F, doppler_settings->altitude / 10E3F, tcp_worker->tx_req->tx_sampling_freq, tcp_worker->tx_req->tx_center_freq, tcp_worker->tx_req->tx_offset, 0, max_output_buffer,
                              tle, &tcp_worker->dopp);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create tx doppler correction block\n", tcp_worker->id);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            return;
        }
    } else if (tcp_worker->tx_req->tx_offset != 0) {
        code = sig_source_create(1.0F, tcp_worker->tx_req->tx_sampling_freq, max_output_buffer, &tcp_worker->signal);
        if (code != 0) {
            fprintf(stderr, "<3>[%d] unable to create freq correction block\n", tcp_worker->id);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            return;
        }
    }
    if (tcp_worker->tx_req->tx_dump_file) {
        char file_path[4096];
        snprintf(file_path, sizeof(file_path), "%s/tx.mod2sdr.%d.cf32", server->server_config->base_path, tcp_worker->id);
        tcp_worker->tx_dump_file = fopen(file_path, "wb");
        if (tcp_worker->tx_dump_file == NULL) {
            fprintf(stderr, "<3>[%d] unable to open file for tx output: %s\n", tcp_worker->id, file_path);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
            tcp_worker_destroy(tcp_worker);
            return;
        }
    }
    if (server->server_config->tx_sdr_type == TX_SDR_TYPE_PLUTOSDR) {
        pthread_mutex_lock(&server->mutex);
        code = tcp_server_init_tx_device(tcp_worker->id, tcp_worker->tx_req, server, &tcp_worker->tx_device);
        pthread_mutex_unlock(&server->mutex);
        if (code < 0) {
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, -code);
            tcp_worker_destroy(tcp_worker);
            return;
        }
    }

    tcp_worker->is_running = true;

    pthread_t client_thread;
    code = pthread_create(&client_thread, NULL, &tcp_worker_callback, tcp_worker);
    if (code != 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }
    tcp_worker->client_thread = client_thread;

    code = linked_list_add(tcp_worker, &tcp_worker_destroy, &server->tcp_workers);
    if (code != 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    api_utils_write_response(tcp_worker->client_socket, RESPONSE_STATUS__SUCCESS, tcp_worker->id);
    fprintf(stdout, "[%d] mod: %s, tx center_freq: %d, tx sampling rate: %d, baud: %d\n", tcp_worker->id, protobuf_c_enum_descriptor_get_value(&modem_type__descriptor, tcp_worker->tx_req->mod_type)->name, tcp_worker->tx_req->tx_center_freq, tcp_worker->tx_req->tx_sampling_freq,
            tcp_worker->tx_req->mod_baud_rate);
}

void handle_rx_client(int client_socket, struct message_header *header, tcp_server *server) {
    struct tcp_worker *tcp_worker = malloc(sizeof(struct tcp_worker));
    if (tcp_worker == NULL) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        return;
    }
    *tcp_worker = (struct tcp_worker) {0};
    tcp_worker->id = server->client_counter;
    tcp_worker->client_socket = client_socket;
    tcp_worker->server = server;
    //explicitly init all tx fields to NULL for rx client
    tcp_worker->tx_req = NULL;
    tcp_worker->fsk_mod = NULL;
    tcp_worker->fsk_mod = NULL;
    tcp_worker->dopp = NULL;
    tcp_worker->tx_dump_file = NULL;
    tcp_worker->tx_device = NULL;

    tcp_worker->buffer_size = 0;
    tcp_worker->buffer = NULL;

    if (api_utils_read_rx_request(client_socket, header, &tcp_worker->rx_req) != 0) {
        fprintf(stderr, "<3>[%d] unable to read request fully\n", tcp_worker->id);
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    if (validate_rx_request(tcp_worker->rx_req, tcp_worker->id, server->server_config) < 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    tcp_worker->is_running = true;

    pthread_t client_thread;
    int code = pthread_create(&client_thread, NULL, &tcp_worker_callback, tcp_worker);
    if (code != 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }
    tcp_worker->client_thread = client_thread;

    dsp_worker *dsp_worker = NULL;
    code = dsp_worker_create(tcp_worker->id, tcp_worker->client_socket, server->server_config, tcp_worker->rx_req, &dsp_worker);
    if (code != 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        return;
    }

    pthread_mutex_lock(&server->mutex);
    code = tcp_server_init_rx_device(dsp_worker, server, tcp_worker);
    pthread_mutex_unlock(&server->mutex);
    if (code != 0) {
        tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, -code);
        // this will trigger sdr_worker destroy if any
        tcp_worker_destroy(tcp_worker);
        dsp_worker_destroy(dsp_worker);
        return;
    }

    api_utils_write_response(tcp_worker->client_socket, RESPONSE_STATUS__SUCCESS, tcp_worker->id);
    fprintf(stdout, "[%d] demod: %s, rx freq: %d, rx sampling_rate: %d, baud: %d, destination: %s\n", tcp_worker->id,
            protobuf_c_enum_descriptor_get_value(&modem_type__descriptor, tcp_worker->rx_req->demod_type)->name, (tcp_worker->rx_req->rx_center_freq + tcp_worker->rx_req->rx_offset),
            tcp_worker->rx_req->rx_sampling_freq, tcp_worker->rx_req->demod_baud_rate, protobuf_c_enum_descriptor_get_value(&demod_destination__descriptor, tcp_worker->rx_req->demod_destination)->name);
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
        if (api_utils_read_header(client_socket, &header) != 0) {
            fprintf(stderr, "<3>[%d] unable to read request header fully\n", server->client_counter);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
            continue;
        }
        if (header.protocol_version != PROTOCOL_VERSION) {
            fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", server->client_counter, header.protocol_version);
            tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
            continue;
        }

        cleanup_terminated_threads(server);

        switch (header.type) {
            case TYPE_RX_REQUEST:
                log_client(&address, server->client_counter);
                handle_rx_client(client_socket, &header, server);
                break;
            case TYPE_TX_REQUEST:
                log_client(&address, server->client_counter);
                handle_tx_client(client_socket, &header, server);
                break;
            case TYPE_PING:
                tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__SUCCESS, RESPONSE_NO_DETAILS);
                break;
            default:
                fprintf(stderr, "<3>[%d] unsupported request: %d\n", server->client_counter, header.type);
                tcp_server_write_response_and_close(client_socket, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
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
    result->tx_initialized = false;
    result->rx_initialized = false;
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
        fprintf(stderr, "invalid address: %s\n", config->bind_address);
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
