
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"
#include "api.pb-c.h"

int api_utils_read_header(int socket, struct message_header *header);

int api_utils_read_modem_request(int socket, const struct message_header *header, struct ModemRequest **request);

int api_utils_read_tx_data(int socket, const struct message_header *header, struct TxData **request);

int api_utils_write_response(int socket, ResponseStatus status, uint32_t details);

// generic accessors for the fields common to every modem type (gfsk/bpsk/dpsk/sdpsk/oqpsk/psk_pm).
// return 0 if req->modem_settings_case is MODEM_REQUEST__MODEM_SETTINGS__NOT_SET or unrecognized.
uint64_t api_utils_get_center_freq(const struct ModemRequest *req);

uint64_t api_utils_get_sample_rate(const struct ModemRequest *req);

uint32_t api_utils_get_baud_rate(const struct ModemRequest *req);

#endif //SDR_MODEM_API_UTILS_H
