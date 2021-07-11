#ifndef API_H_
#define API_H_

#include <stdint.h>

#define PROTOCOL_VERSION 0

// client to server
#define TYPE_RX_REQUEST 0
#define TYPE_SHUTDOWN 1
#define TYPE_PING 3
#define TYPE_TX_DATA 4
#define TYPE_TX_REQUEST 5
//server to client
#define TYPE_RESPONSE 2

#define RESPONSE_NO_DETAILS 0
#define RESPONSE_DETAILS_INVALID_REQUEST 1
#define RESPONSE_DETAILS_INTERNAL_ERROR 3
#define RESPONSE_DETAILS_TX_IS_BEING_USED 4
#define RESPONSE_DETAILS_RX_IS_BEING_USED 5

struct message_header {
    uint8_t protocol_version;
    uint8_t type;
    uint32_t message_length;
} __attribute__((packed));

#endif /* API_H_ */
