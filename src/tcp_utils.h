#ifndef SDR_MODEM_TCP_UTILS_H
#define SDR_MODEM_TCP_UTILS_H

#include <stdlib.h>
#include <stdint.h>

int tcp_utils_write_data(uint8_t *buffer, size_t total_len_bytes, int client_socket);

int tcp_utils_read_data(void *result, size_t len_bytes, int client_socket);

#endif //SDR_MODEM_TCP_UTILS_H
