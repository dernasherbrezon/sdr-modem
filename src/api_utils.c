#include "api_utils.h"
#include "tcp_utils.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

uint32_t MAX_MESSAGE_LENGTH = 32 * 1024; // kilobyte

int api_utils_read_header(int socket, struct message_header *header) {
    int code = tcp_utils_read_data(header, sizeof(struct message_header), socket);
    if (code == 0) {
        header->message_length = ntohl(header->message_length);
    }
    return code;
}

int api_utils_read_tx_data(int socket, const struct message_header *header, struct TxData **request) {
    if (header->message_length > MAX_MESSAGE_LENGTH) {
        return -1;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(buffer, header->message_length, socket);
    if (code != 0) {
        free(buffer);
        return -1;
    }
    TxData *result = tx_data__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_read_tx_request(int socket, const struct message_header *header, struct TxRequest **request) {
    if (header->message_length > MAX_MESSAGE_LENGTH) {
        return -1;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(buffer, header->message_length, socket);
    if (code != 0) {
        return -1;
    }
    TxRequest *result = tx_request__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_read_rx_request(int socket, const struct message_header *header, struct RxRequest **request) {
    if (header->message_length > MAX_MESSAGE_LENGTH) {
        return -1;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(buffer, header->message_length, socket);
    if (code != 0) {
        return -1;
    }
    RxRequest *result = rx_request__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_write_response(int socket, ResponseStatus status, uint32_t details) {
    Response response = RESPONSE__INIT;
    response.status = status;
    response.details = details;

    size_t len = response__get_packed_size(&response);
    if (len > UINT32_MAX) {
        return -1;
    }

    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_RESPONSE;
    header.message_length = htonl((uint32_t) len);

    size_t buffer_len = sizeof(struct message_header) + sizeof(uint8_t) * len;
    uint8_t *buffer = malloc(buffer_len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    memcpy(buffer, &header, sizeof(struct message_header));
    response__pack(&response, buffer + sizeof(struct message_header));

    int code = tcp_utils_write_data(buffer, buffer_len, socket);
    free(buffer);
    return code;
}

void api_utils_convert_tle(char **tle, char (*output)[80]) {
    for (int i = 0; i < 3; i++) {
        strncpy(output[i], tle[i], 80);
    }
}