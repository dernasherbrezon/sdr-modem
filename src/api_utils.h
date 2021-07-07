
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"
#include "api.pb-c.h"

int api_utils_read_header(int socket, struct message_header *header);

int api_utils_read_rx_request(int socket, struct message_header *header, struct RxRequest **request);

int api_utils_read_tx_request(int socket, struct message_header *header, struct TxRequest **request);

int api_utils_read_tx_data(int socket, struct message_header *header, struct TxData **request);

int api_utils_write_response(int socket, ResponseStatus status, uint32_t details);

void api_utils_convert_tle(char **tle, char (*output)[80]);

#endif //SDR_MODEM_API_UTILS_H
