#include "api.h"

#include <string.h>

// wire format (big-endian): version(1) | type(1) | request_id(4) | message_length(4)

static uint32_t read_u32(const uint8_t *p) {
  return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}

static void write_u32(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t) (value >> 24);
  p[1] = (uint8_t) (value >> 16);
  p[2] = (uint8_t) (value >> 8);
  p[3] = (uint8_t) value;
}

int api_decode_message_header(const uint8_t *buffer, size_t buffer_len, message_header *header) {
  if (buffer == NULL || header == NULL || buffer_len < API_HEADER_SIZE) {
    return -1;
  }
  header->protocol_version = buffer[0];
  header->type = buffer[1];
  header->request_id = read_u32(buffer + 2);
  header->message_length = read_u32(buffer + 6);
  return 0;
}

static uint64_t read_u64(const uint8_t *p) {
  return ((uint64_t) read_u32(p) << 32) | (uint64_t) read_u32(p + 4);
}

static float read_f32(const uint8_t *p) {
  uint32_t bits = read_u32(p);
  float result;
  memcpy(&result, &bits, sizeof(result));
  return result;
}

int api_encode_message_header(const message_header *header, uint8_t *buffer, size_t buffer_len, size_t *written) {
  if (header == NULL || buffer == NULL || written == NULL || buffer_len < API_HEADER_SIZE) {
    return -1;
  }
  buffer[0] = header->protocol_version;
  buffer[1] = header->type;
  write_u32(buffer + 2, header->request_id);
  write_u32(buffer + 6, header->message_length);
  *written = API_HEADER_SIZE;
  return 0;
}

// response payload: status(1) | details(1)

int api_encode_response(const response *resp, uint8_t *buffer, size_t buffer_len, size_t *written) {
  if (resp == NULL || buffer == NULL || written == NULL || buffer_len < API_RESPONSE_SIZE) {
    return -1;
  }
  buffer[0] = resp->status;
  buffer[1] = resp->details;
  *written = API_RESPONSE_SIZE;
  return 0;
}

// soft bits payload: data_len raw signed bytes, no extra framing
int api_encode_soft_bits(const soft_bits *bits, uint8_t *buffer, size_t buffer_len, size_t *written) {
  if (bits == NULL || buffer == NULL || written == NULL) {
    return -1;
  }
  if (buffer_len > 0) {
    if (bits->data == NULL) {
      return -1;
    }
    memcpy(buffer, bits->data, buffer_len);
  }
  *written = buffer_len;
  return 0;
}

// comm_settings payload (big-endian): sdr_channel_config | sdr_modem_settings
// sdr_channel_config: frequency(8) | sample_rate(8) | gain(4)
// sdr_modem_settings: modem_type(1) | modem-specific fields, in struct declaration order.
// floats are IEEE 754 binary32. psk.type is not transmitted: it is derived from modem_type.
#define API_SDR_SIZE (8 + 8 + 4)
#define API_GFSK_SIZE (8 + 4 + 8 + 4 + 4 + 1 + 8 + 4)
#define API_PSK_SIZE (8 + 4 + 4 + 4 + 1 + 4 + 1)
#define API_PSK_PM_SIZE (8 + 4 + 4 + 4 + 4 + 4 + 1 + 4 + 1 + 4)

int api_decode_comm_settings(const uint8_t *buffer, size_t buffer_len, comm_settings *settings) {
  if (buffer == NULL || settings == NULL || buffer_len < API_SDR_SIZE + 1) {
    return -1;
  }
  comm_settings decoded;
  memset(&decoded, 0, sizeof(decoded));
  decoded.sdr_settings.frequency = read_u64(buffer);
  decoded.sdr_settings.sample_rate = read_u64(buffer + 8);
  decoded.sdr_settings.gain = read_f32(buffer + 16);

  const uint8_t *modem_buffer = buffer + API_SDR_SIZE;
  uint8_t modem_type = modem_buffer[0];
  const uint8_t *p = modem_buffer + 1;
  size_t payload_len = buffer_len - API_SDR_SIZE - 1;
  sdr_modem_settings result;
  memset(&result, 0, sizeof(result));
  result.modem_type = modem_type;
  switch (modem_type) {
    case MODEM_TYPE_GFSK:
      if (payload_len < API_GFSK_SIZE) {
        return -1;
      }
      result.modem.gfsk.sample_rate = read_u64(p);
      result.modem.gfsk.baud_rate = read_u32(p + 8);
      result.modem.gfsk.deviation = (int64_t) read_u64(p + 12);
      result.modem.gfsk.bandwidth = read_u32(p + 20);
      result.modem.gfsk.bt = read_f32(p + 24);
      result.modem.gfsk.use_dc_block = p[28];
      result.modem.gfsk.syncword = read_u64(p + 29);
      result.modem.gfsk.syncword_bits = read_u32(p + 37);
      break;
    case MODEM_TYPE_BPSK:
    case MODEM_TYPE_DPSK:
    case MODEM_TYPE_SDPSK:
      if (payload_len < API_PSK_SIZE) {
        return -1;
      }
      result.modem.psk.sample_rate = read_u64(p);
      result.modem.psk.baud_rate = read_u32(p + 8);
      result.modem.psk.bandwidth = read_u32(p + 12);
      result.modem.psk.rrc_beta = read_f32(p + 16);
      result.modem.psk.rrc_delay = p[20];
      result.modem.psk.costas_bandwidth = read_f32(p + 21);
      result.modem.psk.symsync_filter_bank_size = p[25];
      if (modem_type == MODEM_TYPE_BPSK) {
        result.modem.psk.type = BPSK;
      } else if (modem_type == MODEM_TYPE_DPSK) {
        result.modem.psk.type = DPSK;
      } else {
        result.modem.psk.type = SDPSK;
      }
      break;
    case MODEM_TYPE_PSK_PM:
      if (payload_len < API_PSK_PM_SIZE) {
        return -1;
      }
      result.modem.psk_pm.sample_rate = read_u64(p);
      result.modem.psk_pm.subcarrier_frequency = read_u32(p + 8);
      result.modem.psk_pm.carrier_pll_bandwidth = read_f32(p + 12);
      result.modem.psk_pm.baud_rate = read_u32(p + 16);
      result.modem.psk_pm.subcarrier_bandwidth = read_u32(p + 20);
      result.modem.psk_pm.rrc_beta = read_f32(p + 24);
      result.modem.psk_pm.rrc_delay = p[28];
      result.modem.psk_pm.costas_bandwidth = read_f32(p + 29);
      result.modem.psk_pm.symsync_filter_bank_size = p[33];
      result.modem.psk_pm.modulation_index = read_f32(p + 34);
      break;
    default:
      return -1;
  }
  decoded.modem_settings = result;
  *settings = decoded;
  return 0;
}
