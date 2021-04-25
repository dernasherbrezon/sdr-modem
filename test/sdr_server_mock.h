#ifndef SDR_MODEM_SDR_SERVER_MOCK_H
#define SDR_MODEM_SDR_SERVER_MOCK_H

typedef struct sdr_server_mock_t sdr_server_mock;

int sdr_server_mock_create(const char *addr, int port, void (*handler)(int client_socket), sdr_server_mock **server);

void mock_response_success(int client_socket);

void sdr_server_mock_destroy(sdr_server_mock *server);

#endif //SDR_MODEM_SDR_SERVER_MOCK_H
