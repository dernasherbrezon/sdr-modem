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

struct tcp_worker {
    struct client_config *config;
    pthread_t client_thread;
    tcp_server *server;
};

struct client_config {
    struct request *req;
    struct sdr_worker_rx *rx;
    int client_socket;
    uint32_t id;
    atomic_bool is_running;
};


struct tcp_server_t {
    int server_socket;
    volatile sig_atomic_t is_running;
    pthread_t acceptor_thread;
    struct server_config *server_config;
    uint32_t client_counter;

    linked_list *tcp_workers;
    linked_list *sdr_configs;
    pthread_mutex_t mutex;
};

void log_client(struct sockaddr_in *address, uint32_t id) {
    char str[INET_ADDRSTRLEN];
    const char *ptr = inet_ntop(AF_INET, &address->sin_addr, str, sizeof(str));
    printf("[%d] accepted new client from %s:%d\n", id, ptr, ntohs(address->sin_port));
}

int read_struct(int socket, void *result, size_t len) {
    size_t left = len;
    while (left > 0) {
        int received = recv(socket, (char *) result + (len - left), left, 0);
        if (received < 0) {
            // will happen on timeout
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return -errno;
            }
            // SIGINT and SIGTERM handled in main
            // all other signals should be ignored and syscall should be retried
            // For example, EINTR happens when gdb disconnects
            if (errno == EINTR) {
                continue;
            }
            // other types of errors
            // log and disconnect
            perror("unable to read struct");
            return -1;
        }
        // client has closed the socket
        if (received == 0) {
            return -1;
        }
        left -= received;
    }
    return 0;
}

int read_client_config(int client_socket, uint32_t client_id, struct server_config *server_config,
                       struct client_config **config) {
    struct client_config *result = malloc(sizeof(struct client_config));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0
    *result = (struct client_config) {0};
    //FIXME what about de-allocation? request + rx
    struct request *req = NULL;
    if (read_struct(client_socket, &req, sizeof(struct request)) < 0) {
        fprintf(stderr, "<3>[%d] unable to read request fully\n", client_id);
        free(result);
        return -1;
    }
    api_network_to_host(req);
    result->req = req;
    result->client_socket = client_socket;
    result->id = client_id;

    struct sdr_worker_rx *rx = malloc(sizeof(struct sdr_worker_rx));
    if (rx == NULL) {
        free(result);
        return -ENOMEM;
    }
    rx->rx_destination = req->rx_destination;
    rx->rx_sampling_freq = req->rx_sampling_freq;
    rx->rx_center_freq = req->rx_center_freq;
    rx->band_freq = req->rx_sdr_server_band_freq;
    result->rx = rx;

    *config = result;
    return 0;
}

int validate_client_config(struct client_config *config, struct server_config *server_config) {

//	if (config->center_freq == 0) {
//		fprintf(stderr, "<3>[%d] missing center_freq parameter\n", client_id);
//		return -1;
//	}
//	if (config->sampling_rate == 0) {
//		fprintf(stderr, "<3>[%d] missing sampling_rate parameter\n", client_id);
//		return -1;
//	}
//	if (config->band_freq == 0) {
//		fprintf(stderr, "<3>[%d] missing band_freq parameter\n", client_id);
//		return -1;
//	}
//	if (config->destination != REQUEST_DESTINATION_FILE && config->destination != REQUEST_DESTINATION_SOCKET) {
//		fprintf(stderr, "<3>[%d] unknown destination: %d\n", client_id, config->destination);
//		return -1;
//	}
//	uint32_t requested_min_freq = config->center_freq - config->sampling_rate / 2;
//	uint32_t server_min_freq = config->band_freq - server_config->band_sampling_rate / 2;
//	if (requested_min_freq < server_min_freq) {
//		fprintf(stderr, "<3>[%d] requested center freq is out of the band: %u\n", client_id, config->center_freq);
//		return -1;
//	}
//	uint32_t requested_max_freq = config->center_freq + config->sampling_rate / 2;
//	uint32_t server_max_freq = config->band_freq + server_config->band_sampling_rate / 2;
//	if (requested_max_freq > server_max_freq) {
//		fprintf(stderr, "<3>[%d] requested center freq is out of the band: %u\n", client_id, config->center_freq);
//		return -1;
//	}
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
    char *buffer = malloc(total_len);
    memcpy(buffer, &header, sizeof(struct message_header));
    memcpy(buffer + sizeof(struct message_header), &resp, sizeof(struct response));

    size_t left = total_len;
    while (left > 0) {
        int written = write(socket, buffer + (total_len - left), left);
        if (written < 0) {
            perror("unable to write the message");
            free(buffer);
            return -1;
        }
        left -= written;
    }
    free(buffer);
    return 0;
}

void respond_failure(int client_socket, uint8_t status, uint8_t details) {
    write_message(client_socket, status, details); // unable to start device
    close(client_socket);
}

static void *tcp_worker_callback(void *arg) {
    struct tcp_worker *worker = (struct tcp_worker *) arg;
    fprintf(stdout, "[%d] tcp_worker is starting\n", worker->config->id);
    while (worker->config->is_running) {
        struct message_header header;
        int code = read_struct(worker->config->client_socket, &header, sizeof(struct message_header));
        if (code < -1) {
            // read timeout happened. it's ok.
            // client already sent all information we need
            continue;
        }
        if (code == -1) {
            fprintf(stdout, "[%d] client disconnected\n", worker->config->id);
            break;
        }
        if (header.protocol_version != PROTOCOL_VERSION) {
            fprintf(stderr, "<3>[%d] unsupported protocol: %d\n", worker->config->id, header.protocol_version);
            continue;
        }
        if (header.type != TYPE_SHUTDOWN) {
            fprintf(stderr, "<3>[%d] unsupported request: %d\n", worker->config->id, header.type);
            continue;
        }
        fprintf(stdout, "[%d] client requested disconnect\n", worker->config->id);
        break;
    }
    pthread_mutex_lock(&worker->server->mutex);
    uint32_t id = worker->config->id;
    linked_list_destroy_by_id(&id, &sdr_worker_destroy_by_id, &worker->server->sdr_configs);
    pthread_mutex_unlock(&worker->server->mutex);
    worker->config->is_running = false;
    close(worker->config->client_socket);
    return (void *) 0;
}

bool tcp_worker_is_stopped(void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    if (worker->config->is_running) {
        return false;
    }
    return true;
}

void tcp_worker_destroy(void *data) {
    struct tcp_worker *worker = (struct tcp_worker *) data;
    fprintf(stdout, "[%d] tcp_worker is stopping\n", worker->config->id);
    worker->config->is_running = false;
    close(worker->config->client_socket);
    pthread_join(worker->client_thread, NULL);
    free(worker->config);
    free(worker);
}

void cleanup_terminated_threads(tcp_server *server) {
    pthread_mutex_lock(&server->mutex);
    linked_list_destroy_by_selector(&tcp_worker_is_stopped, &server->tcp_workers);
    pthread_mutex_unlock(&server->mutex);
}


void remove_all_tcp_workers(tcp_server *server) {
    pthread_mutex_lock(&server->mutex);
    linked_list_destroy(server->tcp_workers);
    server->tcp_workers = NULL;
    pthread_mutex_unlock(&server->mutex);
}

void handle_new_client(int client_socket, tcp_server *server) {
    struct client_config *config = NULL;
    if (read_client_config(client_socket, server->client_counter, server->server_config, &config) < 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        return;
    }
    if (validate_client_config(config, server->server_config) < 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
        free(config);
        return;
    }

    cleanup_terminated_threads(server);

    config->is_running = true;

    dsp_worker *dsp_worker = NULL;
    int code = dsp_worker_create(config->id, config->client_socket, server->server_config, config->req, &dsp_worker);
    if (code != 0) {
        free(config);
        return;
    }

    struct tcp_worker *tcp_worker = malloc(sizeof(struct tcp_worker));
    if (tcp_worker == NULL) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        dsp_worker_destroy(dsp_worker);
        free(config);
        return;
    }
    tcp_worker->config = config;
    tcp_worker->server = server;

    pthread_t client_thread;
    code = pthread_create(&client_thread, NULL, &tcp_worker_callback, tcp_worker);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        dsp_worker_destroy(dsp_worker);
        tcp_worker_destroy(tcp_worker);
        free(config);
        return;
    }
    tcp_worker->client_thread = client_thread;

    pthread_mutex_lock(&server->mutex);
    code = linked_list_add(tcp_worker, &tcp_worker_destroy, &server->tcp_workers);
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        dsp_worker_destroy(dsp_worker);
        free(config);
        pthread_mutex_lock(&server->mutex);
        return;
    }

    //re-use sdr connections
    //this will allow demodulating different modes using the same data
    sdr_worker *sdr = linked_list_find(config->rx, &sdr_worker_find_closest, server->sdr_configs);
    if (sdr != NULL) {
        code = sdr_worker_add_dsp_worker(dsp_worker, sdr);
    } else {
        code = sdr_worker_create(config->rx, server->server_config->rx_sdr_server_address, server->server_config->rx_sdr_server_port, server->server_config->buffer_size, &sdr);
        if (code == 0) {
            code = linked_list_add(sdr, &sdr_worker_destroy, &server->sdr_configs);
            if (code != 0) {
                // the rest of objects should be destroyed below
                sdr_worker_destroy(sdr);
            }
        }
    }
    if (code != 0) {
        respond_failure(client_socket, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
        tcp_worker_destroy(tcp_worker);
        dsp_worker_destroy(dsp_worker);
        free(config);
    } else {
        write_message(config->client_socket, RESPONSE_STATUS_SUCCESS, config->id);
        fprintf(stdout, "[%d] demod %s rx center_freq %d rx sampling_rate %d rx destination %d\n", config->id,
                api_demod_type_str(config->req->demod_type), config->req->rx_center_freq,
                config->req->rx_sampling_freq, config->req->rx_destination);
    }
    pthread_mutex_unlock(&server->mutex);
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
        if (read_struct(client_socket, &header, sizeof(struct message_header)) < 0) {
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

    remove_all_tcp_workers(server);

    printf("tcp server stopped\n");
    return (void *) 0;
}

int tcp_server_create(struct server_config *config, tcp_server **server) {
    tcp_server *result = malloc(sizeof(struct tcp_server_t));
    if (result == NULL) {
        return -ENOMEM;
    }
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
