#ifndef SDR_SERVER_API_H_
#define SDR_SERVER_API_H_

#include <stdint.h>

#define SDR_SERVER_PROTOCOL_VERSION 0

// client to server
#define SDR_SERVER_TYPE_REQUEST 0
#define SDR_SERVER_TYPE_SHUTDOWN 1
#define SDR_SERVER_TYPE_PING 3
//server to client
#define SDR_SERVER_TYPE_RESPONSE 2

struct sdr_server_message_header {
	uint8_t protocol_version;
	uint8_t type;
} __attribute__((packed));

#define SDR_SERVER_REQUEST_DESTINATION_FILE 0
#define SDR_SERVER_REQUEST_DESTINATION_SOCKET 1

struct sdr_server_request {
	uint32_t center_freq;
	uint32_t sampling_rate;
	uint32_t band_freq;
	uint8_t destination;
} __attribute__((packed));

#define SDR_SERVER_RESPONSE_STATUS_SUCCESS 0
#define SDR_SERVER_RESPONSE_STATUS_FAILURE 1

#define SDR_SERVER_RESPONSE_DETAILS_INVALID_REQUEST 1
#define SDR_SERVER_RESPONSE_DETAILS_OUT_OF_BAND_FREQ 2
#define SDR_SERVER_RESPONSE_DETAILS_INTERNAL_ERROR 3

struct sdr_server_response {
	uint8_t status;
    uint32_t details; // on success contains file index, on error contains error code
} __attribute__((packed));


#endif /* SDR_SERVER_API_H_ */
