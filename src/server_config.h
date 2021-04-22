#ifndef SERVER_CONFIG_H_
#define SERVER_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

#define RX_SDR_TYPE_SDR_SERVER 0

struct server_config {
	// socket settings
	char* bind_address;
	int port;
	int read_timeout_seconds;

	uint32_t buffer_size;
    int queue_size;

    uint8_t rx_sdr_type;

	// output settings
	char *base_path;
};

int server_config_create(struct server_config **config, const char *path);

void server_config_destroy(struct server_config *config);

#endif /* SERVER_CONFIG_H_ */
