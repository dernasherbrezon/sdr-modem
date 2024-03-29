#include "sdr_server_mock.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <complex.h>
#include "../src/sdr/sdr_server_api.h"
#include "../src/tcp_utils.h"

struct sdr_server_mock_t {
    int server_socket;
    volatile sig_atomic_t is_running;
    pthread_t acceptor_thread;

    void (*handler)(int client_socket, sdr_server_mock *server);

    float complex *input;
    size_t input_len;

    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int client_socket;
};

void mock_response_success(int client_socket, sdr_server_mock *server) {
    struct sdr_server_message_header header;
    int code = -2;
    while (code < -1) {
        code = tcp_utils_read_data(&header, sizeof(struct sdr_server_message_header), client_socket);
    }
    if (code != 0) {
        fprintf(stderr, "unable to read header from sdr-modem\n");
        return;
    }
    if (header.protocol_version != SDR_SERVER_PROTOCOL_VERSION) {
        fprintf(stderr, "invalid protocol version: %d\n", header.protocol_version);
        return;
    }
    if (header.type != SDR_SERVER_TYPE_REQUEST) {
        fprintf(stderr, "expected type = request. got: %d\n", header.type);
        return;
    }
    struct sdr_server_request request;
    code = tcp_utils_read_data(&request, sizeof(struct sdr_server_request), client_socket);
    if (code != 0) {
        fprintf(stderr, "unable to read request from sdr-modem\n");
        return;
    }

    header.type = SDR_SERVER_TYPE_RESPONSE;
    header.protocol_version = SDR_SERVER_PROTOCOL_VERSION;

    struct sdr_server_response response;
    response.status = SDR_SERVER_RESPONSE_STATUS_SUCCESS;
    response.details = 0;

    // it is possible to directly populate *buffer with the fields,
    // however populating structs and then serializing them into byte array
    // is more readable
    size_t total_len = sizeof(struct sdr_server_message_header) + sizeof(struct sdr_server_response);
    uint8_t *buffer = malloc(total_len);
    if (buffer == NULL) {
        return;
    }
    memcpy(buffer, &header, sizeof(struct sdr_server_message_header));
    memcpy(buffer + sizeof(struct sdr_server_message_header), &response, sizeof(struct sdr_server_response));
    tcp_utils_write_data(buffer, total_len, client_socket);
    free(buffer);
}

static void *acceptor_worker(void *arg) {
    sdr_server_mock *server = (sdr_server_mock *) arg;
    struct sockaddr_in address;
    while (server->is_running) {
        int client_socket;
        int addrlen = sizeof(address);
        if ((client_socket = accept(server->server_socket, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            break;
        }

        struct timeval tv;
        tv.tv_sec = 5000;
        tv.tv_usec = 0;
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv)) {
            close(client_socket);
            perror("setsockopt - SO_RCVTIMEO");
            continue;
        }

        //doesn't support multiple clients actually
        //no needed for tests
        pthread_mutex_lock(&server->mutex);
        if (server->handler != NULL) {
            server->handler(client_socket, server);
        }
        server->client_socket = client_socket;
        pthread_cond_broadcast(&server->condition);
        pthread_mutex_unlock(&server->mutex);
    }

    printf("sdr server mock stopped\n");
    if (server->client_socket >= 0) {
        close(server->client_socket);
    }
    return (void *) 0;
}

int sdr_server_mock_send(float complex *input, size_t input_len, sdr_server_mock *server) {
    pthread_mutex_lock(&server->mutex);
    while (server->client_socket < 0) {
        pthread_cond_wait(&server->condition, &server->mutex);
    }
    pthread_mutex_unlock(&server->mutex);
    return tcp_utils_write_data((uint8_t *) input, sizeof(float complex) * input_len, server->client_socket);
}

int sdr_server_mock_create(const char *addr, int port, void (*handler)(int client_socket, sdr_server_mock *server), uint32_t max_output_buffer_length, sdr_server_mock **server) {
    struct sdr_server_mock_t *result = malloc(sizeof(struct sdr_server_mock_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct sdr_server_mock_t) {0};

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        free(result);
        perror("socket creation failed");
        return -1;
    }
    result->server_socket = server_socket;
    result->is_running = true;
    result->handler = handler;
    result->input_len = max_output_buffer_length;
    result->input = malloc(sizeof(float complex) * result->input_len);
    if (result->input == NULL) {
        return -ENOMEM;
    }
    result->condition = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    result->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    result->client_socket = -1;

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
    if (inet_pton(AF_INET, addr, &address.sin_addr) <= 0) {
        free(result);
        perror("invalid address");
        return -1;
    }
    address.sin_port = htons(port);

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

void sdr_server_mock_destroy(sdr_server_mock *server) {
    if (server == NULL) {
        return;
    }
    server->is_running = false;
    // close is not enough to exit from the blocking "accept" method
    // execute shutdown first
    int code = shutdown(server->server_socket, SHUT_RDWR);
    if (code != 0) {
        close(server->server_socket);
    }
    pthread_join(server->acceptor_thread, NULL);

    if (server->input != NULL) {
        free(server->input);
    }
    free(server);
}