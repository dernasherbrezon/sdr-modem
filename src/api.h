#ifndef API_H_
#define API_H_

#include <stdint.h>
#include <stddef.h>
#include "dsp/sdr_modem.h"
#include "sdr/sdr_device.h"

#define PROTOCOL_VERSION 0

typedef enum {
  MESSAGE_TYPE_RX_COMM_PARAMETERS = 0,
  MESSAGE_TYPE_SHUTDOWN = 1,
  MESSAGE_TYPE_PING = 2,
  MESSAGE_TYPE_TX_COMM_PARAMETERS = 3,
  MESSAGE_TYPE_RESPONSE = 4,
  MESSAGE_TYPE_SOFT_BITS = 5,
  MESSAGE_TYPE_TX_FRAME = 6,
  MESSAGE_TYPE_RX_START = 7,
  MESSAGE_TYPE_RX_STOP = 8,
  MESSAGE_TYPE_TX_COMPLETE = 9,
  MESSAGE_TYPE_CARRIER_DETECTED = 10,
  MESSAGE_TYPE_CARRIER_LOST = 11
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
  RESPONSE_DETAILS_INVALID_REQUEST = 1
} response_details;

#define API_RESPONSE_SIZE 2

typedef struct {
  uint8_t status;
  uint8_t details;
} response;

#define API_SOFT_BITS_HEADER_SIZE 8

// payload: frequency_offset(4, signed, Hz) | data_length(4) | data (raw signed bytes)
typedef struct {
  int32_t frequency_offset;
  int8_t *data;
  uint32_t data_length;
} soft_bits;

typedef struct {
  sdr_channel_config sdr_settings;
  sdr_modem_settings modem_settings;
} comm_parameters;

int api_decode_message_header(const uint8_t *buffer, size_t buffer_len, message_header *header);

int api_encode_message_header(const message_header *header, uint8_t *buffer, size_t buffer_len, size_t *written);

// buffer_len must be at least API_SOFT_BITS_HEADER_SIZE + bits->data_length
int api_encode_soft_bits(const soft_bits *bits, uint8_t *buffer, size_t buffer_len, size_t *written);

int api_decode_comm_settings(const uint8_t *buffer, size_t buffer_len, comm_parameters *settings);

int api_encode_comm_settings(const comm_parameters *settings, uint8_t *buffer, size_t buffer_len, size_t *written);

int api_decode_response(const uint8_t *buffer, size_t buffer_len, response *resp);

int api_encode_response(const response *resp, uint8_t *buffer, size_t buffer_len, size_t *written);

#endif /* API_H_ */
