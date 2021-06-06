#include "tcp_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

int tcp_utils_write_data(uint8_t *buffer, size_t total_len_bytes, int client_socket) {
    size_t left = total_len_bytes;
    while (left > 0) {
        ssize_t written = write(client_socket, buffer + (total_len_bytes - left), left);
        if (written < 0) {
            return -1;
        }
        left -= written;
    }
    return 0;
}

int tcp_utils_read_data_partially(void *result, size_t len_bytes, size_t *actually_read, int client_socket) {
    size_t left = len_bytes;
    int code = 0;
    while (left > 0) {
        ssize_t received = recv(client_socket, (char *) result + (len_bytes - left), left, 0);
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                code = -errno;
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            code = -1;
            break;
        }
        // client has closed the socket
        if (received == 0) {
            code = -1;
            break;
        }
        left -= received;
    }
    *actually_read = len_bytes - left;
    return code;
}

int tcp_utils_read_data(void *result, size_t len_bytes, int client_socket) {
    size_t actually_read = 0;
    return tcp_utils_read_data_partially(result, len_bytes, &actually_read, client_socket);
}
