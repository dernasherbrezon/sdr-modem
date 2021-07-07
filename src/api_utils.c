#include "api_utils.h"
#include "tcp_utils.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

uint32_t MAX_MESSAGE_LENGTH = 32 * 1024; // kilobyte

int api_utils_read_header(int socket, struct message_header *header) {
    int code = tcp_utils_read_data(header, sizeof(struct message_header), socket);
    if (code == 0) {
        header->message_length = ntohl(header->message_length);
    }
    return code;
}

int api_utils_read_tx_data(int socket, struct message_header *header, struct TxData **request) {
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
    TxData *result = tx_data__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_read_tx_request(int socket, struct message_header *header, struct TxRequest **request) {
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

int api_utils_read_rx_request(int socket, struct message_header *header, struct RxRequest **request) {
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

    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_RESPONSE;
    header.message_length = htonl(len);

    int code = tcp_utils_write_data((uint8_t *) &header, sizeof(struct message_header), socket);
    if (code != 0) {
        return code;
    }

    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    response__pack(&response, buffer);
    code = tcp_utils_write_data(buffer, len, socket);
    free(buffer);
    return code;
}

void api_utils_convert_tle(char **tle, char (*output)[80]) {
    for (int i = 0; i < 3; i++) {
        strncpy(output[i], tle[i], 80);
    }
}