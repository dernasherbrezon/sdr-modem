
#ifndef SDR_MODEM_API_UTILS_H
#define SDR_MODEM_API_UTILS_H

#include "api.h"

void api_network_to_host(struct request *req);

const char * api_modem_type_str(int demod_type);

#endif //SDR_MODEM_API_UTILS_H
