#include "api_utils.h"
#include <stdlib.h>

void api_network_to_host(struct request *req) {
    if (req == NULL) {
        return;
    }
    req->rx_center_freq = ntohl(req->rx_center_freq);
    req->rx_sampling_freq = ntohl(req->rx_sampling_freq);
    req->rx_sdr_server_band_freq = ntohl(req->rx_sdr_server_band_freq);
    req->latitude = ntohl(req->latitude);
    req->longitude = ntohl(req->longitude);
    req->altitude = ntohl(req->altitude);
    req->demod_baud_rate = ntohl(req->demod_baud_rate);
    req->demod_fsk_deviation = ntohl(req->demod_fsk_deviation);
    req->demod_fsk_transition_width = ntohl(req->demod_fsk_transition_width);
    req->tx_center_freq = ntohl(req->tx_center_freq);
    req->tx_sampling_freq = ntohl(req->tx_sampling_freq);
    req->mod_baud_rate = ntohl(req->mod_baud_rate);
    req->mod_fsk_deviation = ntohl(req->mod_fsk_deviation);
}

const char *api_demod_type_str(int demod_type) {
    if (demod_type == REQUEST_MODEM_TYPE_FSK) {
        return "FSK";
    } else if (demod_type == REQUEST_MODEM_TYPE_NONE) {
        return "NONE";
    }
    return "UNKNOWN";
}