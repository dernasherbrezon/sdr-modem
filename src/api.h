#ifndef API_H_
#define API_H_

#include <stdint.h>

#define PROTOCOL_VERSION 0

typedef enum {
  MESSAGE_TYPE_RX_REQUEST = 0,
  MESSAGE_TYPE_SHUTDOWN,
  MESSAGE_TYPE_PING,
  MESSAGE_TYPE_TX_DATA,
  MESSAGE_TYPE_TX_REQUEST,
  MESSAGE_TYPE_RESPONSE,
  MESSAGE_TYPE_SOFT_SYMBOLS
} message_type;

typedef enum {
  RESPONSE_DETAILS_NO_DETAILS = 0,
  RESPONSE_DETAILS_INVALID_REQUEST,
  RESPONSE_DETAILS_INTERNAL_ERROR,
  RESPONSE_DETAILS_TX_IS_BEING_USED,
  RESPONSE_DETAILS_RX_IS_BEING_USED
} response_details;

typedef struct {
  uint8_t protocol_version;
  uint8_t type;
  uint32_t request_id;
  uint32_t message_length;
} __attribute__((packed)) message_header;

#endif /* API_H_ */
