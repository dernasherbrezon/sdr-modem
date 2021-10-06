#include "sdr_modem_client.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include "../src/tcp_utils.h"
#include "../src/api_utils.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

struct sdr_modem_client_t {
    int client_socket;

    int8_t *output;
    size_t output_len;
};

int sdr_modem_client_write_tx_request(struct message_header *header, struct TxRequest *req, sdr_modem_client *client) {
    size_t len = tx_request__get_packed_size(req);
    header->message_length = htonl(len);
    int code = tcp_utils_write_data((uint8_t *) header, sizeof(struct message_header), client->client_socket);
    if (code != 0) {
        return code;
    }

    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    tx_request__pack(req, buffer);
    code = tcp_utils_write_data(buffer, len, client->client_socket);
    free(buffer);
    return code;
}

int sdr_modem_client_write_request(struct message_header *header, struct RxRequest *req, sdr_modem_client *client) {
    size_t len = rx_request__get_packed_size(req);
    header->message_length = htonl(len);
    int code = tcp_utils_write_data((uint8_t *) header, sizeof(struct message_header), client->client_socket);
    if (code != 0) {
        return code;
    }

    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    rx_request__pack(req, buffer);
    code = tcp_utils_write_data(buffer, len, client->client_socket);
    free(buffer);
    return code;
}

int sdr_modem_client_write_tx_raw(struct message_header *header, struct TxData *req, uint32_t req_len, sdr_modem_client *client) {
    if (req->data.len < req_len) {
        fprintf(stderr, "cannot simulate more data than actually allocated\n");
        return -1;
    }
    size_t len = tx_data__get_packed_size(req);
    header->message_length = htonl(len);
    int code = tcp_utils_write_data((uint8_t *) header, sizeof(struct message_header), client->client_socket);
    if (code != 0) {
        return code;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    tx_data__pack(req, buffer);
    // simulation partial payload
    // always send header
    code = tcp_utils_write_data(buffer, len - req->data.len + req_len, client->client_socket);
    free(buffer);
    return code;
}

int sdr_modem_client_write_tx(struct message_header *header, struct TxData *req, sdr_modem_client *client) {
    return sdr_modem_client_write_tx_raw(header, req, req->data.len, client);
}

int sdr_modem_client_write_raw(uint8_t *buffer, size_t buffer_len, sdr_modem_client *client) {
    return tcp_utils_write_data(buffer, buffer_len, client->client_socket);
}

int sdr_modem_client_read_response(struct message_header **response_header, struct Response **resp, sdr_modem_client *client) {
    struct message_header *header = malloc(sizeof(struct message_header));
    if (header == NULL) {
        return -ENOMEM;
    }
    int code = api_utils_read_header(client->client_socket, header);
    if (code != 0) {
        free(header);
        return code;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        free(header);
        return -ENOMEM;
    }

    code = tcp_utils_read_data(buffer, header->message_length, client->client_socket);
    if (code != 0) {
        free(buffer);
        free(header);
        return code;
    }
    Response *result = response__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        free(header);
        return -1;
    }

    *response_header = header;
    *resp = result;
    return 0;
}

int sdr_modem_client_read_stream(int8_t **output, size_t expected_read, sdr_modem_client *client) {
    if (expected_read > client->output_len) {
        fprintf(stderr, "<3>requested buffer %zu is more than max: %zu\n", expected_read, client->output_len);
        return -1;
    }
    int code = tcp_utils_read_data(client->output, sizeof(int8_t) * expected_read, client->client_socket);
    if (code != 0) {
        return code;
    }
    *output = client->output;
    return 0;
}

int sdr_modem_client_create(const char *addr, int port, uint32_t max_buffer_length, int read_timeout_seconds, sdr_modem_client **client) {
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

    struct timeval tv;
    tv.tv_sec = read_timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(result->client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv)) {
        perror("setsockopt - SO_RCVTIMEO");
        sdr_modem_client_destroy(result);
        return -1;
    }

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
    fprintf(stdout, "connected to sdr-modem..\n");

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
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_SHUTDOWN;
    header.message_length = 0;
    int code = tcp_utils_write_data((uint8_t *) &header, sizeof(struct message_header), client->client_socket);
    if (code != 0) {
        printf("invalid response while sending shutdown: %d\n", code);
    } else {
        while (true) {
            code = tcp_utils_read_data(&header, sizeof(struct message_header), client->client_socket);
            if (code < -1) {
                // read timeout happened. it's ok.
                // client already sent all information we need
                continue;
            }
            if (code == -1) {
                break;
            }
        }
    }
    fprintf(stdout, "disconnected from the server..\n");
    sdr_modem_client_destroy(client);
}