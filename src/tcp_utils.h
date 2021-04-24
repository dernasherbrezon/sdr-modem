#ifndef SDR_MODEM_TCP_UTILS_H
#define SDR_MODEM_TCP_UTILS_H

#include <stdlib.h>

int tcp_utils_write_data(void *buffer, size_t total_len, int client_socket);

int tcp_utils_read_data(void *result, size_t len, int client_socket);

#endif //SDR_MODEM_TCP_UTILS_H
