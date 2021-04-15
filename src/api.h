#ifndef API_H_
#define API_H_

#include <stdint.h>

#define PROTOCOL_VERSION 0

// client to server
#define TYPE_REQUEST 0
#define TYPE_SHUTDOWN 1
#define TYPE_PING 3
//server to client
#define TYPE_RESPONSE 2

struct message_header {
	uint8_t protocol_version;
	uint8_t type;
} __attribute__((packed));

#define REQUEST_RX_DESTINATION_FILE 0
#define REQUEST_RX_DESTINATION_SOCKET 1

#define REQUEST_DEMOD_FSK_USE_DC_BLOCK_NO 0
#define REQUEST_DEMOD_FSK_USE_DC_BLOCK_YES 1

#define REQUEST_DEMOD_TYPE_FSK 0

struct request {

    //generic RX SDR settings
	uint32_t rx_center_freq;
	uint32_t rx_sampling_freq;
	uint8_t rx_destination;

	//sdr-server RX settings
    uint32_t rx_sdr_server_band_freq;

    //generic demodulator settings
    uint8_t demod_type;
	uint32_t demod_baud_rate;
    uint8_t demod_decimation;

    //FSK demodulator settings
	float demod_fsk_deviation;
    uint32_t demod_fsk_transition_width;
    uint8_t demod_fsk_use_dc_block;

} __attribute__((packed));

#define RESPONSE_STATUS_SUCCESS 0
#define RESPONSE_STATUS_FAILURE 1

#define RESPONSE_DETAILS_INVALID_REQUEST 1
#define RESPONSE_DETAILS_INTERNAL_ERROR 3

struct response {
	uint8_t status;
	uint8_t details; // on success contains file index, on error contains error code
} __attribute__((packed));


#endif /* API_H_ */
