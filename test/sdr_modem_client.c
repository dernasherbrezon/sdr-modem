#include "sdr_modem_client.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include "../src/tcp_utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

struct sdr_modem_client_t {
    int client_socket;

    int8_t *output;
    size_t output_len;
};

int sdr_modem_write_request(struct message_header *header, struct request *req, sdr_modem_client *client) {
    req->rx_center_freq = htonl(req->rx_center_freq);
    req->rx_sampling_freq = htonl(req->rx_sampling_freq);
    req->rx_sdr_server_band_freq = htonl(req->rx_sdr_server_band_freq);
    req->latitude = htonl(req->latitude);
    req->longitude = htonl(req->longitude);
    req->altitude = htonl(req->altitude);
    req->demod_baud_rate = htonl(req->demod_baud_rate);
    req->demod_fsk_deviation = htonl(req->demod_fsk_deviation);
    req->demod_fsk_transition_width = htonl(req->demod_fsk_transition_width);

    size_t total_len = sizeof(struct message_header) + sizeof(struct request);
    uint8_t *buffer = malloc(total_len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    memcpy(buffer, header, sizeof(struct message_header));
    memcpy(buffer + sizeof(struct message_header), req, sizeof(struct request));

    int code = tcp_utils_write_data(buffer, total_len, client->client_socket);
    free(buffer);
    return code;
}

int sdr_modem_read_response(struct message_header **response_header, struct response **resp, sdr_modem_client *client) {
    struct message_header *header = malloc(sizeof(struct message_header));
    if (header == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(header, sizeof(struct message_header), client->client_socket);
    if (code != 0) {
        free(header);
        return code;
    }
    struct response *result = malloc(sizeof(struct response));
    if (result == NULL) {
        free(header);
        return -ENOMEM;
    }
    code = tcp_utils_read_data(result, sizeof(struct response), client->client_socket);
    if (code != 0) {
        free(header);
        free(result);
        return code;
    }
    *response_header = header;
    *resp = result;
    return 0;
}

int sdr_modem_read_stream(int8_t **output, size_t *output_len, sdr_modem_client *client) {
    int code = tcp_utils_read_data(client->output, sizeof(int8_t) * client->output_len, client->client_socket);
    if (code != 0) {
        return code;
    }
    *output = client->output;
    *output_len = client->output_len;
    return 0;
}

int sdr_modem_client_create(const char *addr, int port, uint32_t max_buffer_length, sdr_modem_client **client) {
    struct sdr_modem_client_t *result = malloc(sizeof(struct sdr_modem_client_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct sdr_modem_client_t) {0};

    result->output_len = max_buffer_length;
    result->output = malloc(sizeof(int8_t) * result->output_len);
    if (result->output == NULL) {
        sdr_modem_client_destroy(result);
        return -ENOMEM;
    }

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        sdr_modem_client_destroy(result);
        fprintf(stderr, "<3>socket creation failed: %d\n", client_socket);
        return -1;
    }
    result->client_socket = client_socket;

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(addr);
    address.sin_port = htons(port);
    int code = connect(client_socket, (struct sockaddr *) &address, sizeof(address));
    if (code != 0) {
        sdr_modem_client_destroy(result);
        fprintf(stderr, "<3>connection with the server failed: %d\n", code);
        return -1;
    }
    fprintf(stdout, "connected to the server..\n");

    *client = result;
    return 0;
}

void sdr_modem_client_destroy(sdr_modem_client *client) {
    if (client == NULL) {
        return;
    }
    if (client->output != NULL) {
        free(client->output);
    }
    close(client->client_socket);
    free(client);
}

void sdr_modem_client_destroy_gracefully(sdr_modem_client *client) {
    while (true) {
        struct message_header header;
        int code = tcp_utils_read_data(&header, sizeof(struct message_header), client->client_socket);
        if (code < -1) {
            // read timeout happened. it's ok.
            // client already sent all information we need
            continue;
        }
        if (code == -1) {
            break;
        }
    }
    fprintf(stdout, "disconnected from the server..\n");
    sdr_modem_client_destroy(client);
}