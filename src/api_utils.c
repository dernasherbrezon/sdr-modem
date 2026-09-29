#include "api_utils.h"
#include "tcp_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

uint32_t MAX_MESSAGE_LENGTH = 32 * 1024; // kilobyte

int api_utils_read_header(int socket, struct message_header *header) {
    int code = tcp_utils_read_data(header, sizeof(struct message_header), socket);
    if (code == 0) {
        header->message_length = ntohl(header->message_length);
    }
    return code;
}

int api_utils_read_tx_data(int socket, const struct message_header *header, struct TxData **request) {
    if (header->message_length > MAX_MESSAGE_LENGTH) {
        return -1;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(buffer, header->message_length, socket);
    if (code != 0) {
        free(buffer);
        return -1;
    }
    TxData *result = tx_data__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_read_modem_request(int socket, const struct message_header *header, struct ModemRequest **request) {
    if (header->message_length > MAX_MESSAGE_LENGTH) {
        return -1;
    }
    uint8_t *buffer = malloc(sizeof(uint8_t) * header->message_length);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    int code = tcp_utils_read_data(buffer, header->message_length, socket);
    if (code != 0) {
        return -1;
    }
    ModemRequest *result = modem_request__unpack(NULL, header->message_length, buffer);
    free(buffer);
    if (result == NULL) {
        return -1;
    }
    *request = result;
    return 0;
}

int api_utils_write_response(int socket, ResponseStatus status, uint32_t details) {
    Response response = RESPONSE__INIT;
    response.status = status;
    response.details = details;

    size_t len = response__get_packed_size(&response);
    if (len > UINT32_MAX) {
        return -1;
    }

    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_RESPONSE;
    header.message_length = htonl((uint32_t) len);

    size_t buffer_len = sizeof(struct message_header) + sizeof(uint8_t) * len;
    uint8_t *buffer = malloc(buffer_len);
    if (buffer == NULL) {
        return -ENOMEM;
    }
    memcpy(buffer, &header, sizeof(struct message_header));
    response__pack(&response, buffer + sizeof(struct message_header));

    int code = tcp_utils_write_data(buffer, buffer_len, socket);
    free(buffer);
    return code;
}

uint64_t api_utils_get_sample_rate(const struct ModemRequest *req) {
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      return req->gfsk->sample_rate;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      // bpsk/dpsk/sdpsk share the same settings message (union aliasing), so req->bpsk works for all 3
      return req->bpsk->sample_rate;
    case MODEM_REQUEST__MODEM_SETTINGS_PSK_PM:
      return req->psk_pm->sample_rate;
    default:
      return 0;
  }
}

uint32_t api_utils_get_baud_rate(const struct ModemRequest *req) {
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      return req->gfsk->baud_rate;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      return req->bpsk->baud_rate;
    case MODEM_REQUEST__MODEM_SETTINGS_PSK_PM:
      return req->psk_pm->baud_rate;
    default:
      return 0;
  }
}

static void api_utils_convert_psk(const PskModemSettings *req, psk_modem_type type, bpsk_modem_settings *settings) {
  settings->sample_rate = req->sample_rate;
  settings->baud_rate = req->baud_rate;
  settings->rrc_beta = req->rrc_beta;
  settings->rrc_delay = req->rrc_delay;
  settings->costas_bandwidth = req->costas_bandwidth;
  settings->symsync_filter_bank_size = req->symsync_filter_bank_size;
  settings->bandwidth = req->bandwidth;
  settings->type = type;
}

int api_utils_convert_modem_request(const struct ModemRequest *req, int *modem_type, sdr_modem_settings *settings) {
  *settings = (sdr_modem_settings){0};
  switch (req->modem_settings_case) {
    case MODEM_REQUEST__MODEM_SETTINGS__NOT_SET:
      *modem_type = MODEM_TYPE_NONE;
      return 0;
    case MODEM_REQUEST__MODEM_SETTINGS_GFSK:
      *modem_type = MODEM_TYPE_GFSK;
      settings->gfsk.sample_rate = req->gfsk->sample_rate;
      settings->gfsk.baud_rate = req->gfsk->baud_rate;
      settings->gfsk.deviation = req->gfsk->deviation;
      settings->gfsk.bandwidth = req->gfsk->bandwidth;
      settings->gfsk.bt = req->gfsk->bt;
      settings->gfsk.use_dc_block = req->gfsk->use_dc_block;
      settings->gfsk.syncword = req->syncword;
      settings->gfsk.syncword_bits = req->syncword_bits;
      return 0;
    case MODEM_REQUEST__MODEM_SETTINGS_BPSK:
      *modem_type = MODEM_TYPE_BPSK;
      api_utils_convert_psk(req->bpsk, BPSK, &settings->psk);
      return 0;
    case MODEM_REQUEST__MODEM_SETTINGS_DPSK:
      *modem_type = MODEM_TYPE_DPSK;
      api_utils_convert_psk(req->dpsk, DPSK, &settings->psk);
      return 0;
    case MODEM_REQUEST__MODEM_SETTINGS_SDPSK:
      *modem_type = MODEM_TYPE_SDPSK;
      api_utils_convert_psk(req->sdpsk, SDPSK, &settings->psk);
      return 0;
    case MODEM_REQUEST__MODEM_SETTINGS_PSK_PM:
      *modem_type = MODEM_TYPE_PSK_PM;
      settings->psk_pm.sample_rate = req->psk_pm->sample_rate;
      settings->psk_pm.baud_rate = req->psk_pm->baud_rate;
      settings->psk_pm.rrc_beta = req->psk_pm->rrc_beta;
      settings->psk_pm.rrc_delay = req->psk_pm->rrc_delay;
      settings->psk_pm.costas_bandwidth = req->psk_pm->costas_bandwidth;
      settings->psk_pm.symsync_filter_bank_size = req->psk_pm->symsync_filter_bank_size;
      settings->psk_pm.subcarrier_frequency = req->psk_pm->subcarrier_frequency;
      settings->psk_pm.modulation_index = req->psk_pm->modulation_index;
      settings->psk_pm.carrier_pll_bandwidth = req->psk_pm->carrier_pll_bandwidth;
      settings->psk_pm.subcarrier_bandwidth = req->psk_pm->subcarrier_bandwidth;
      return 0;
    default:
      fprintf(stderr, "<3>unsupported modem type: %d\n", req->modem_settings_case);
      return -1;
  }
}
