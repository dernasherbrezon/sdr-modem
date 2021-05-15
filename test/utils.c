#include "utils.h"
#include <check.h>
#include <volk/volk.h>
#include <stdio.h>

struct request *create_request() {
    struct request *result = malloc(sizeof(struct request));
    ck_assert(result != NULL);
    result->rx_sampling_freq = 48000;
    result->rx_sdr_server_band_freq = 437525000;
    result->rx_center_freq = 437525000;
    result->rx_dump_file = REQUEST_RX_DUMP_FILE_NO;
    result->altitude = 0;
    char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    memcpy(result->tle, tle, sizeof(tle));
    result->latitude = 53.72 * 10E6;
    result->longitude = 47.57F * 10E6;
    result->correct_doppler = REQUEST_CORRECT_DOPPLER_YES;
    result->demod_destination = REQUEST_DEMOD_DESTINATION_SOCKET;
    result->demod_type = REQUEST_MODEM_TYPE_FSK;
    result->demod_fsk_use_dc_block = REQUEST_DEMOD_FSK_USE_DC_BLOCK_YES;
    result->demod_fsk_transition_width = 2000;
    result->demod_decimation = 2;
    result->demod_fsk_deviation = 5000;
    result->demod_baud_rate = 4800;
    return result;
}

void setup_input_data(float **input, size_t input_offset, size_t len) {
    float *result = malloc(sizeof(float) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (input_offset + i);
    }
    *input = result;
}

void setup_volk_input_data(float **input, size_t input_offset, size_t len) {
    float *result = volk_malloc(sizeof(float) * len, volk_get_alignment());
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (input_offset + i);
    }
    *input = result;
}

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len) {
    float complex *result = malloc(sizeof(float complex) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (float) (2 * input_offset + 2 * i) + (float) (2 * input_offset + 2 * i + 1) * I;
    }
    *input = result;
}

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
        ck_assert(fabsl(expected[i] - crealf(actual[j])) < 0.001);
        ck_assert(fabsl(expected[i + 1] - cimagf(actual[j])) < 0.001);
    }
}

void assert_byte_array(const int8_t expected[], size_t expected_size, int8_t *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; i++) {
        ck_assert_int_eq(expected[i], actual[i]);
    }
}

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; i++) {
        ck_assert(fabsl(expected[i] - actual[i]) < 0.001);
    }
}