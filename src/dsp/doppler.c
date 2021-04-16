#include "doppler.h"
#include "../sgpsdp/sgp4sdp4.h"
#include <errno.h>

struct doppler_t {
    geodetic_t ground_station;
};

int doppler_create(float latitude, float longitude, float altitude, uint32_t rx_center_freq, char tle[3][80], doppler **d) {
    struct doppler_t *result = malloc(sizeof(struct doppler_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    // init all fields with 0 so that destroy_* method would work
    *result = (struct doppler_t) {0};
    result->ground_station.lat = latitude;
    result->ground_station.lon = longitude;
    result->ground_station.alt = altitude;
    result->ground_station.theta = 0.0; //FIXME ???

    *d = result;
    return 0;
}

void doppler_process(float complex *input, size_t input_len, float complex **output, size_t output_len, doppler *result) {

}

void doppler_destroy(doppler *result) {
    if (result == NULL) {
        return;
    }
    free(result);
}