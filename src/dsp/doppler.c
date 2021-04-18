#include "doppler.h"
#include "../sgpsdp/sgp4sdp4.h"
#include "sig_source.h"
#include <errno.h>
#include <volk/volk.h>
#include <sys/time.h>

float SPEED_OF_LIGHT = 2.99792458E8;

struct doppler_t {
    sig_source *source;
    uint32_t sampling_freq;
    uint32_t center_freq;
    double jul_start_time;

    uint32_t current_freq_difference;
    uint32_t next_freq_difference;
    float freq_difference_per_sample;

    uint32_t update_interval_samples;
    uint64_t next_update_samples;
    uint64_t current_samples;

    geodetic_t *ground_station;
    sat_t *satellite;
    obs_set_t *obs_set;

    float complex *output;
    size_t output_len;
};

uint32_t doppler_calculate_shift(doppler *result) {
    double tsince = (result->satellite->jul_utc - result->satellite->jul_epoch) * xmnpda;
    if (result->satellite->flags & DEEP_SPACE_EPHEM_FLAG) {
        SDP4(result->satellite, tsince);
    } else {
        SGP4(result->satellite, tsince);
    }

    Convert_Sat_State(&result->satellite->pos, &result->satellite->vel);
    Calculate_Obs(tsince, &result->satellite->pos, &result->satellite->vel, result->ground_station, result->obs_set);
    return result->center_freq - result->center_freq * (SPEED_OF_LIGHT - result->obs_set->range_rate) / SPEED_OF_LIGHT;
}

int doppler_create(float latitude, float longitude, float altitude, uint32_t sampling_freq, uint32_t center_freq, time_t start_time_seconds, uint32_t max_output_buffer_length, char tle[3][80], doppler **d) {
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
    result->center_freq = center_freq;
    if (start_time_seconds == 0) {
        result->jul_start_time = 0.0;
    } else {
        struct tm *cdate = gmtime(&start_time_seconds);
        cdate->tm_year += 1900;
        cdate->tm_mon += 1;
        result->jul_start_time = Julian_Date(&cdate);
    }
    result->current_freq_difference = 0;
    result->freq_difference_per_sample = 0.0F;
    result->next_update_samples = 0;
    result->current_samples = 0;
    result->sampling_freq = sampling_freq;
    result->update_interval_samples = sampling_freq * 1.0F; // recalculate doppler frequency every second
    // no idea what theta means
    result->ground_station->theta = 0.0;
    int code = sig_source_create(1.0F, sampling_freq, max_output_buffer_length, &result->source);
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
    result->satellite->jul_epoch = Julian_Date_of_Epoch(result->satellite->tle.epoch);
    result->next_freq_difference = 0;
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
    // update correction frequency every X samples.
    // This would assume sample rate is taken evenly during the second
    if (result->current_samples >= result->next_update_samples) {
        result->next_update_samples = result->current_samples + result->update_interval_samples;
        if (result->next_freq_difference == 0) {
            // start time can be 0
            // in that case correction is happening in realtime
            // take current system time
            // all calculations should be done lazily, because there is no guarantee doppler_process be executed right after doppler_create
            if (result->jul_start_time == 0.0) {
                struct tm t;
                UTC_Calendar_Now(&t);
                result->satellite->jul_utc = Julian_Date(&t);
            } else {
                result->satellite->jul_utc = result->jul_start_time;
            }
            result->current_freq_difference = doppler_calculate_shift(result);
        } else {
            result->current_freq_difference = result->next_freq_difference;
        }

        result->satellite->jul_utc = result->jul_start_time + result->next_update_samples / (double) result->sampling_freq;
        result->next_freq_difference = doppler_calculate_shift(result);
        // linear interpolation between next and current doppler shift
        // this is to avoid sudden jumps of frequency between corrections
        result->freq_difference_per_sample = (result->next_freq_difference - result->current_freq_difference) / result->sampling_freq;
    } else {
        result->current_samples += result->update_interval_samples;
        result->current_freq_difference += result->freq_difference_per_sample * input_len;
    }

    float complex *sig_output = NULL;
    size_t sig_output_len = 0;
    sig_source_process(result->current_freq_difference, input_len, &sig_output, &sig_output_len, result->source);

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