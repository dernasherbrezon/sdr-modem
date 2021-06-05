#ifndef SDR_MODEM_SDR_SERVER_MOCK_H
#define SDR_MODEM_SDR_SERVER_MOCK_H

#include <stdint.h>
#include <complex.h>
#include <stdlib.h>

typedef struct sdr_server_mock_t sdr_server_mock;

int sdr_server_mock_create(const char *addr, int port, void (*handler)(int client_socket, sdr_server_mock *server), uint32_t max_output_buffer_length, sdr_server_mock **server);

void mock_response_success(int client_socket, sdr_server_mock *server);

int sdr_server_mock_send(float complex *input, size_t input_len, sdr_server_mock *server);

void sdr_server_mock_destroy(sdr_server_mock *server);

#endif //SDR_MODEM_SDR_SERVER_MOCK_H
