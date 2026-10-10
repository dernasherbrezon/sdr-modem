#ifndef API_H_
#define API_H_

#include <stdint.h>
#include <stddef.h>
#include "dsp/sdr_modem.h"

#define PROTOCOL_VERSION 0

typedef enum {
  MESSAGE_TYPE_RX_COMM_PARAMETERS = 0,
  MESSAGE_TYPE_SHUTDOWN = 1,
  MESSAGE_TYPE_PING = 2,
  MESSAGE_TYPE_TX_COMM_PARAMETERS = 3,
  MESSAGE_TYPE_RESPONSE = 4,
  MESSAGE_TYPE_SOFT_BITS = 5
} message_type;

#define API_HEADER_SIZE 10

typedef struct {
  uint8_t protocol_version;
  uint8_t type;
  uint32_t request_id;
  uint32_t message_length;
} message_header;

typedef enum {
  RESPONSE_STATUS_SUCCESS = 0,
  RESPONSE_STATUS_FAILURE = 1
} response_status;

typedef enum {
  RESPONSE_DETAILS_NO_DETAILS = 0,
  RESPONSE_DETAILS_INVALID_REQUEST = 1,
  RESPONSE_DETAILS_INTERNAL_ERROR = 2,
  RESPONSE_DETAILS_TX_IS_BEING_USED = 3,
  RESPONSE_DETAILS_RX_IS_BEING_USED = 4
} response_details;

#define API_RESPONSE_SIZE 2

typedef struct {
  uint8_t status;
  uint8_t details;
} response;

// doesn't really need encoding
// just a byte array after the header
typedef struct {
  int8_t *data;
} soft_bits;

int api_decode_message_header(const uint8_t *buffer, size_t buffer_len, message_header *header);

int api_encode_message_header(const message_header *header, uint8_t *buffer, size_t buffer_len, size_t *written);

int api_decode_sdr_modem_settings(const uint8_t *buffer, size_t buffer_len, sdr_modem_settings *settings);

int api_encode_response(const response *resp, uint8_t *buffer, size_t buffer_len, size_t *written);

#endif /* API_H_ */
