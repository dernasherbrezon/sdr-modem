
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"

void normalize_rx_request(struct rx_request *req);

void normalize_tx_request(struct tx_request *req);

const char *api_modem_type_str(int demod_type);

#endif //SDR_MODEM_API_UTILS_H
