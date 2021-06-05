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
            perror("<3>unable to write the message");
            return -1;
        }
        left -= written;
    }
    return 0;
}

int tcp_utils_read_data(void *result, size_t len_bytes, int client_socket) {
    size_t left = len_bytes;
    while (left > 0) {
        ssize_t received = recv(client_socket, (char *) result + (len_bytes - left), left, 0);
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return -errno;
            }
            if (errno == EINTR) {
                continue;
            }
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
