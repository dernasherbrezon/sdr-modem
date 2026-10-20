#ifndef SDR_MODEM_SDR_MODEM_CLIENT_H
#define SDR_MODEM_SDR_MODEM_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include "../src/api.h"

// returned by sdr_modem_client_receive when nothing arrived within the timeout
#define SDR_MODEM_CLIENT_TIMEOUT (-2)

// blocking tcp client of the sdr-modem server. It speaks the protocol of src/api.h
typedef struct sdr_modem_client_t sdr_modem_client;

int sdr_modem_client_create(const char *host, uint16_t port, int timeout_seconds, sdr_modem_client **client);

// low level: any version, type and payload, including the invalid ones.
// payload == NULL sends only the header, which claims payload_len bytes
int sdr_modem_client_send(uint8_t protocol_version, uint8_t type, uint32_t request_id, const uint8_t *payload, size_t payload_len, sdr_modem_client *client);

int sdr_modem_client_set_rx_parameters(uint32_t request_id, const comm_settings *settings, sdr_modem_client *client);

int sdr_modem_client_set_tx_parameters(uint32_t request_id, const comm_settings *settings, sdr_modem_client *client);

int sdr_modem_client_rx_start(uint32_t request_id, sdr_modem_client *client);

int sdr_modem_client_rx_stop(uint32_t request_id, sdr_modem_client *client);

int sdr_modem_client_ping(uint32_t request_id, sdr_modem_client *client);

int sdr_modem_client_tx_frame(uint32_t request_id, const uint8_t *frame, size_t frame_len, sdr_modem_client *client);

// asks the server to drop the connection
int sdr_modem_client_shutdown(uint32_t request_id, sdr_modem_client *client);

// reads the next message. The payload is valid until the next call.
// Returns 0, SDR_MODEM_CLIENT_TIMEOUT or -1 if the server closed the connection
int sdr_modem_client_receive(message_header *header, const uint8_t **payload, sdr_modem_client *client);

// reads messages until the response for request_id arrives. Soft bits received on the way are counted, but not returned
int sdr_modem_client_wait_response(uint32_t request_id, response *resp, sdr_modem_client *client);

// reads messages until at least min_bytes of soft bits are received in total. Other messages are skipped
int sdr_modem_client_wait_soft_bits(size_t min_bytes, sdr_modem_client *client);

// total payload size of all soft bits messages received so far
size_t sdr_modem_client_soft_bits_received(const sdr_modem_client *client);

void sdr_modem_client_destroy(sdr_modem_client *client);

#endif //SDR_MODEM_SDR_MODEM_CLIENT_H
