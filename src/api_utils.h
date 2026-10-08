
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"
#include "api.pb-c.h"
#include "dsp/sdr_modem.h"

int api_utils_read_header(int socket, message_header *header);

int api_utils_read_modem_request(int socket, const message_header *header, struct ModemRequest **request);

int api_utils_read_tx_data(int socket, const message_header *header, struct TxData **request);

// generic accessor for the field common to every modem type (gfsk/bpsk/dpsk/sdpsk/oqpsk/psk_pm).
// returns 0 if req->modem_settings_case is MODEM_REQUEST__MODEM_SETTINGS__NOT_SET or unrecognized.
uint32_t api_utils_get_baud_rate(const struct ModemRequest *req);

// converts protobuf request into the native modem settings. *modem_type is one of MODEM_TYPE_* and
// selects the populated member of settings. MODEM_TYPE_NONE if modem settings are not set.
// returns non-zero if the modem type is not supported
int api_utils_convert_modem_request(const struct ModemRequest *req, sdr_modem_type *modem_type, sdr_modem_settings *settings);

#endif //SDR_MODEM_API_UTILS_H
