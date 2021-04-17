#include "doppler.h"
#include "../sgpsdp/sgp4sdp4.h"
#include "sig_source.h"
#include <errno.h>
#include <volk/volk.h>

float SPEED_OF_LIGHT = 2.99792458E8;

struct doppler_t {
    sig_source *source;
    uint32_t rx_sampling_freq;
    uint32_t rx_center_freq;
    uint32_t rx_corrected_center_freq;

    geodetic_t *ground_station;
    sat_t *satellite;
    obs_set_t *obs_set;

    float complex *output;
    size_t output_len;
};

int doppler_create(float latitude, float longitude, float altitude, uint32_t rx_sampling_freq, uint32_t rx_center_freq, uint32_t max_output_buffer_length, char tle[3][80], doppler **d) {
    struct doppler_t *result = malloc(sizeof(struct doppler_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct doppler_t) {0};
    result->ground_station = malloc(sizeof(geodetic_t));
    if (result->ground_station == NULL) {
        doppler_destroy(result);
        return -ENOMEM;
    }
    result->ground_station->lat = latitude;
    result->ground_station->lon = longitude;
    result->ground_station->alt = altitude;
    // no idea what theta means
    result->ground_station->theta = 0.0;
    int code = sig_source_create(1.0F, rx_sampling_freq, max_output_buffer_length, &result->source);
    if (code != 0) {
        doppler_destroy(result);
        return code;
    }
    result->obs_set = malloc(sizeof(obs_set_t));
    if (result->obs_set == NULL) {
        doppler_destroy(result);
        return -ENOMEM;
    }
    result->satellite = malloc(sizeof(sat_t));
    if (result->satellite == NULL) {
        doppler_destroy(result);
        return -ENOMEM;
    }
    code = Get_Next_Tle_Set(tle, &result->satellite->tle);
    // yes yes. 1 is for success
    if (code != 1) {
        doppler_destroy(result);
        return -1;
    }
    select_ephemeris(result->satellite);
    result->output_len = max_output_buffer_length;
    result->output = malloc((sizeof(float complex) * result->output_len));
    if (result->output == NULL) {
        doppler_destroy(result);
        return -ENOMEM;
    }
    *d = result;
    return 0;
}

void doppler_process(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result) {
    if (result->satellite->flags & DEEP_SPACE_EPHEM_FLAG) {
        SDP4(result->satellite, 0);
    } else {
        SGP4(result->satellite, 0);
    }

    Convert_Sat_State(&result->satellite->pos, &result->satellite->vel);
    Calculate_Obs(result->satellite->jul_utc, &result->satellite->pos, &result->satellite->vel, result->ground_station, result->obs_set);
    result->rx_corrected_center_freq = result->rx_center_freq * (SPEED_OF_LIGHT - result->obs_set->range_rate) / SPEED_OF_LIGHT;

    //FIXME update freq every X configured seconds

    float complex *sig_output = NULL;
    size_t sig_output_len = 0;
    sig_source_process(result->rx_corrected_center_freq, input_len, &sig_output, &sig_output_len, result->source);

    volk_32fc_x2_multiply_32fc((lv_32fc_t *) result->output, (const lv_32fc_t *) input, (const lv_32fc_t *) sig_output, input_len);

    *output = result->output;
    *output_len = input_len;
}

void doppler_destroy(doppler *result) {
    if (result == NULL) {
        return;
    }
    if (result->ground_station != NULL) {
        free(result->ground_station);
    }
    if (result->source != NULL) {
        sig_source_destroy(result->source);
    }
    if (result->satellite != NULL) {
        free(result->satellite);
    }
    if (result->output != NULL) {
        free(result->output);
    }
    if (result->obs_set != NULL) {
        free(result->obs_set);
    }
    free(result);
}