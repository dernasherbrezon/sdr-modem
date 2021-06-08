#ifndef API_H_
#define API_H_

#include <stdint.h>

#define PROTOCOL_VERSION 0

// client to server
#define TYPE_REQUEST 0
#define TYPE_SHUTDOWN 1
#define TYPE_PING 3
#define TYPE_TX_DATA 4
//server to client
#define TYPE_RESPONSE 2

struct message_header {
    uint8_t protocol_version;
    uint8_t type;
} __attribute__((packed));

#define REQUEST_DUMP_FILE_NO 0
#define REQUEST_DUMP_FILE_YES 1

#define REQUEST_DEMOD_FSK_USE_DC_BLOCK_NO 0
#define REQUEST_DEMOD_FSK_USE_DC_BLOCK_YES 1

#define REQUEST_CORRECT_DOPPLER_NO 0
#define REQUEST_CORRECT_DOPPLER_YES 1

#define REQUEST_MODEM_TYPE_NONE 0
#define REQUEST_MODEM_TYPE_FSK 1

#define REQUEST_DEMOD_DESTINATION_FILE 0
#define REQUEST_DEMOD_DESTINATION_SOCKET 1
#define REQUEST_DEMOD_DESTINATION_BOTH 2

struct request {

    //generic RX SDR settings
    uint32_t rx_center_freq;
    uint32_t rx_sampling_freq;
    uint8_t rx_dump_file;

    //sdr-server RX settings
    uint32_t rx_sdr_server_band_freq;

    //doppler-related settings
    uint8_t correct_doppler;
    char tle[3][80];
    uint32_t latitude;      //degrees times 10^6
    uint32_t longitude;     //degrees times 10^6
    uint32_t altitude;      //kilometers times 10^6

    //generic demodulator settings
    uint8_t demod_type;
    uint32_t demod_baud_rate;
    uint8_t demod_decimation;
    uint8_t demod_destination;

    //FSK demodulator settings
    int32_t demod_fsk_deviation;
    uint32_t demod_fsk_transition_width;
    uint8_t demod_fsk_use_dc_block;

    // generic TX sdr settings
    uint32_t tx_center_freq;
    uint32_t tx_sampling_freq;
    uint8_t tx_dump_file;

    //generic modulator settings
    uint8_t mod_type;
    uint32_t mod_baud_rate;

    //FSK modulator settings
    int32_t mod_fsk_deviation;

} __attribute__((packed));

#define RESPONSE_STATUS_SUCCESS 0
#define RESPONSE_STATUS_FAILURE 1

#define RESPONSE_DETAILS_INVALID_REQUEST 1
#define RESPONSE_DETAILS_INTERNAL_ERROR 3

struct response {
    uint8_t status;
    uint32_t details; // on success contains file index, on error contains error code
} __attribute__((packed));

// not used really, here for the clearness of API
struct tx_data {
    uint32_t len;
    uint8_t *data;
};

#endif /* API_H_ */
