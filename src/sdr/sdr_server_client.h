#ifndef SDR_SERVER_CLIENT_H_
#define SDR_SERVER_CLIENT_H_

#include <complex.h>
#include <stdint.h>
#include "sdr_server_api.h"
#include "sdr_device.h"

typedef struct sdr_server_client_t sdr_server_client;

typedef struct {
  char *addr;               // sdr-server hostname or ip address
  int port;                 // sdr-server port
  int read_timeout_seconds; // socket read timeout
  uint64_t frequency;       // center frequency to request from sdr-server, in Hz
  uint64_t sample_rate;     // sample rate to request from sdr-server, in Hz
} sdr_server_settings;

int sdr_server_client_create(uint32_t id, const sdr_server_settings *settings, uint32_t max_output_buffer_length, sdr_device **result);

int sdr_server_client_read_stream(float complex **output, size_t *output_len, void *plugin);

int sdr_server_client_request(struct sdr_server_request request, struct sdr_server_response **response, sdr_server_client *client);

void sdr_server_client_stop(void *plugin);

void sdr_server_client_destroy(void *plugin);

#endif /* SDR_SERVER_CLIENT_H_ */
