#include "doppler.h"
#include "../sgpsdp/sgp4sdp4.h"
#include "sig_source.h"
#include <errno.h>

// km/sec
double SPEED_OF_LIGHT = 2.99792458E5;

struct doppler_t {
    sig_source *source;
    uint64_t sampling_freq;
    int64_t center_freq;
    int64_t constant_offset;
    double jul_start_time;

    int64_t current_freq_difference;
    int64_t next_freq_difference;
    float freq_difference_per_sample;

    uint64_t update_interval_samples;
    uint64_t next_update_samples;
    uint64_t current_samples;

    geodetic_t *ground_station;
    sat_t *satellite;
    obs_set_t *obs_set;

};

int64_t doppler_calculate_shift(doppler *result, int direction) {
    double tsince = (result->satellite->jul_utc - result->satellite->jul_epoch) * xmnpda;
    if (result->satellite->flags & DEEP_SPACE_EPHEM_FLAG) {
        SDP4(result->satellite, tsince);
    } else {
        SGP4(result->satellite, tsince);
    }

    Convert_Sat_State(&result->satellite->pos, &result->satellite->vel);
    Calculate_Obs(result->satellite->jul_utc, &result->satellite->pos, &result->satellite->vel, result->ground_station, result->obs_set);
    return (int64_t) (direction * (result->center_freq - result->center_freq * (SPEED_OF_LIGHT - result->obs_set->range_rate) / SPEED_OF_LIGHT)) + result->constant_offset;
}

int doppler_create(double latitude, double longitude, double altitude, uint64_t sampling_freq, uint64_t center_freq, int64_t constant_offset, time_t start_time_seconds, uint32_t max_output_buffer_length, char tle[3][80], doppler **d) {
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
    result->ground_station->lat = Radians(latitude);
    result->ground_station->lon = Radians(longitude);
    result->ground_station->alt = altitude;
    result->center_freq = (int64_t) center_freq;
    result->constant_offset = constant_offset;
    if (start_time_seconds == 0) {
        result->jul_start_time = 0.0;
    } else {
        struct tm cdate;
        if (gmtime_r(&start_time_seconds, &cdate) == NULL) {
            doppler_destroy(result);
            return -1;
        }
        cdate.tm_year += 1900;
        cdate.tm_mon += 1;
        result->jul_start_time = Julian_Date(&cdate);
    }
    result->current_freq_difference = 0;
    result->freq_difference_per_sample = 0.0F;
    result->next_update_samples = 0;
    result->current_samples = 0;
    result->sampling_freq = sampling_freq;
    result->update_interval_samples = sampling_freq; // * 1.0F recalculate doppler frequency every second
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
    *result->satellite = (sat_t) {0};
    code = Get_Next_Tle_Set(tle, &result->satellite->tle);
    // yes yes. 1 is for success
    if (code != 1) {
        fprintf(stderr, "<3>invalid tle configuration\n");
        doppler_destroy(result);
        return -1;
    }
    select_ephemeris(result->satellite);
    result->satellite->jul_epoch = Julian_Date_of_Epoch(result->satellite->tle.epoch);
    result->next_freq_difference = 0;
    *d = result;
    return 0;
}

void doppler_process(float complex *input, size_t input_len, float complex **output, size_t *output_len, int direction, doppler *result) {
    if (input == NULL || input_len == 0) {
        *output = NULL;
        *output_len = 0;
        return;
    }
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
                result->jul_start_time = Julian_Date(&t);
            }
            result->satellite->jul_utc = result->jul_start_time;
            result->current_freq_difference = doppler_calculate_shift(result, direction);
        } else {
            result->current_freq_difference = result->next_freq_difference;
        }

        result->satellite->jul_utc += (double) result->update_interval_samples / result->sampling_freq / secday;
        result->next_freq_difference = doppler_calculate_shift(result, direction);
        // linear interpolation between next and current doppler shift
        // this is to avoid sudden jumps of frequency between corrections
        result->freq_difference_per_sample = ((float) result->next_freq_difference - result->current_freq_difference) / result->update_interval_samples;
    } else {
        result->current_samples += input_len;
        result->current_freq_difference += result->freq_difference_per_sample * input_len;
    }

    float complex *sig_output = NULL;
    size_t sig_output_len = 0;
    sig_source_multiply(result->current_freq_difference, input, input_len, &sig_output, &sig_output_len, result->source);

    *output = sig_output;
    *output_len = sig_output_len;
}

void doppler_process_tx(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result) {
    doppler_process(input, input_len, output, output_len, -1, result);
}

void doppler_process_rx(float complex *input, size_t input_len, float complex **output, size_t *output_len, doppler *result) {
    doppler_process(input, input_len, output, output_len, 1, result);
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
    if (result->obs_set != NULL) {
        free(result->obs_set);
    }
    free(result);
}