#include "api_utils.h"
#include <arpa/inet.h>
#include <stdlib.h>

void api_network_to_host(struct request *req) {
    if( req == NULL ) {
        return;
    }
    req->rx_center_freq = ntohl(req->rx_center_freq);
    req->rx_sampling_rate = ntohl(req->rx_sampling_rate);
    req->rx_destination = req->rx_destination;
    req->rx_sdr_server_band_freq = ntohl(req->rx_sdr_server_band_freq);
    req->demod_type = req->demod_type;
    req->demod_baud_rate = ntohl(req->demod_baud_rate);
    req->demod_decimation = req->demod_decimation;
    //FIXME float?
    req->demod_fsk_deviation = req->demod_fsk_deviation;
    req->demod_fsk_transition_width = ntohl(req->demod_fsk_transition_width);
    req->demod_fsk_use_dc_block = req->demod_fsk_use_dc_block;
}

const char * api_demod_type_str(int demod_type) {
    if( demod_type == REQUEST_DEMOD_TYPE_FSK ) {
        return "FSK";
    }
    return "UNKNOWN";
}