
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"
#include "api.pb-c.h"

int api_utils_read_header(int socket, struct message_header *header);

int api_utils_read_modem_request(int socket, const struct message_header *header, struct ModemRequest **request);

int api_utils_read_tx_data(int socket, const struct message_header *header, struct TxData **request);

int api_utils_write_response(int socket, ResponseStatus status, uint32_t details);

#endif //SDR_MODEM_API_UTILS_H
