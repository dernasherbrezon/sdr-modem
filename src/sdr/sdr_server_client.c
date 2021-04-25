#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "../tcp_utils.h"
#include "sdr_server_client.h"

struct sdr_server_client_t {
    int client_socket;

    float complex *output;
    size_t output_len;
};

int sdr_server_client_create(char *addr, int port, uint32_t max_output_buffer_length, sdr_server_client **client) {
    struct sdr_server_client_t *result = malloc(sizeof(struct sdr_server_client_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct sdr_server_client_t) {0};

    result->output_len = max_output_buffer_length;
    result->output = malloc(sizeof(float complex) * result->output_len);
    if (result->output == NULL) {
        free(result);
        return -ENOMEM;
    }

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        free(result->output);
        free(result);
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
        free(result->output);
        free(result);
        fprintf(stderr, "<3>connection with the server failed: %d\n", code);
        return -1;
    }
    fprintf(stdout, "connected to the sdr server..\n");

    *client = result;
    return 0;
}

int sdr_server_client_read_response(struct sdr_server_response **response, sdr_server_client *client) {
    struct sdr_server_message_header *header = malloc(sizeof(struct sdr_server_message_header));
    if (header == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(header, sizeof(struct sdr_server_message_header), client->client_socket);
    if (code != 0) {
        free(header);
        return code;
    }
    if (header->protocol_version != SDR_SERVER_PROTOCOL_VERSION) {
        fprintf(stderr, "<3>unsupported protocol version: %d\n", header->protocol_version);
        free(header);
        return -1;
    }
    if (header->type != SDR_SERVER_TYPE_RESPONSE) {
        fprintf(stderr, "<3>unsupported message type: %d\n", header->type);
        free(header);
        return -1;
    }
    // header is not used
    free(header);
    struct sdr_server_response *result = malloc(sizeof(struct sdr_server_response));
    if (result == NULL) {
        return -ENOMEM;
    }
    code = tcp_utils_read_data(result, sizeof(struct sdr_server_response), client->client_socket);
    if (code != 0) {
        free(result);
        return code;
    }
    *response = result;
    return 0;
}

int sdr_server_client_read_stream(float complex **output, size_t *output_len, sdr_server_client *client) {
    int code = tcp_utils_read_data(client->output, sizeof(float complex) * client->output_len, client->client_socket);
    if (code != 0) {
        return code;
    }
    *output = client->output;
    *output_len = client->output_len;
    return 0;
}

int sdr_server_client_request(struct sdr_server_request request, struct sdr_server_response **response, sdr_server_client *client) {
    struct sdr_server_message_header header;
    header.type = SDR_SERVER_TYPE_REQUEST;
    header.protocol_version = SDR_SERVER_PROTOCOL_VERSION;

    request.band_freq = htonl(request.band_freq);
    request.center_freq = htonl(request.center_freq);
    request.sampling_rate = htonl(request.sampling_rate);
    // it is possible to directly populate *buffer with the fields,
    // however populating structs and then serializing them into byte array
    // is more readable
    size_t total_len = sizeof(struct sdr_server_message_header) + sizeof(struct sdr_server_request);
    uint8_t *buffer = malloc(total_len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    memcpy(buffer, &header, sizeof(struct sdr_server_message_header));
    memcpy(buffer + sizeof(struct sdr_server_message_header), &request, sizeof(struct sdr_server_request));
    int code = tcp_utils_write_data(buffer, total_len, client->client_socket);
    free(buffer);
    if (code != 0) {
        return code;
    }
    return sdr_server_client_read_response(response, client);
}

void sdr_server_client_destroy(sdr_server_client *client) {
    if (client == NULL) {
        return;
    }
    while (true) {
        struct sdr_server_message_header header;
        int code = tcp_utils_read_data(&header, sizeof(struct sdr_server_message_header), client->client_socket);
        if (code != 0) {
            break;
        }
    }
    fprintf(stdout, "disconnected from the server..\n");
    close(client->client_socket);
    if (client->output != NULL) {
        free(client->output);
    }
    free(client);
}

