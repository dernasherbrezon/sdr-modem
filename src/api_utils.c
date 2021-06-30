#include "api_utils.h"
#include <stdlib.h>
#include <arpa/inet.h>

void normalize_rx_request(struct rx_request *req) {
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
}

void normalize_tx_request(struct tx_request *req) {
    if (req == NULL) {
        return;
    }
    req->tx_center_freq = ntohl(req->tx_center_freq);
    req->tx_sampling_freq = ntohl(req->tx_sampling_freq);
    req->mod_baud_rate = ntohl(req->mod_baud_rate);
    req->mod_fsk_deviation = ntohl(req->mod_fsk_deviation);
}

const char *api_modem_type_str(int demod_type) {
    if (demod_type == REQUEST_MODEM_TYPE_FSK) {
        return "FSK";
    }
    return "UNKNOWN";
}

const char *demod_destination_type_str(int demod_destination) {
    switch (demod_destination) {
        case REQUEST_DEMOD_DESTINATION_FILE:
            return "FILE";
        case REQUEST_DEMOD_DESTINATION_SOCKET:
            return "SOCKET";
        case REQUEST_DEMOD_DESTINATION_BOTH:
            return "BOTH";
        default:
            return "UNKNOWN";
    }
}