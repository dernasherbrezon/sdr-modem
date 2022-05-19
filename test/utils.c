#include "utils.h"
#include <check.h>
#include <volk/volk.h>
#include <errno.h>

struct RxRequest *create_rx_request() {
    struct DopplerSettings doppler_settings = DOPPLER_SETTINGS__INIT;
    doppler_settings.altitude = 0;
    doppler_settings.n_tle = 3;
    char **tle = malloc(sizeof(char *) * doppler_settings.n_tle);
    tle[0] = "LUCKY-7";
    tle[1] = "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992";
    tle[2] = "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524";
    doppler_settings.tle = tle;
    doppler_settings.latitude = 53.72 * 10E6;
    doppler_settings.longitude = 47.57F * 10E6;

    struct FskDemodulationSettings fsk_settings = FSK_DEMODULATION_SETTINGS__INIT;
    fsk_settings.demod_fsk_use_dc_block = true;
    fsk_settings.demod_fsk_transition_width = 2000;
    fsk_settings.demod_fsk_deviation = 5000;

    struct RxRequest result = RX_REQUEST__INIT;
    result.rx_sampling_freq = 48000;
    result.rx_offset = 0;
    result.rx_center_freq = 437525000;
    result.rx_dump_file = false;
    result.doppler = &doppler_settings;
    result.demod_destination = DEMOD_DESTINATION__SOCKET;
    result.demod_type = MODEM_TYPE__GMSK;
    result.demod_decimation = 2;
    result.demod_baud_rate = 4800;
    result.fsk_settings = &fsk_settings;
    struct FileSettings rx_file_settings = FILE_SETTINGS__INIT;
    rx_file_settings.filename = utils_read_and_copy_str("tx.cf32");
    rx_file_settings.start_time_seconds = 0L;
    result.file_settings = &rx_file_settings;

    size_t len = rx_request__get_packed_size(&result);
    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        free(tle);
        return NULL;
    }
    rx_request__pack(&result, buffer);
    struct RxRequest *unpacked = rx_request__unpack(NULL, len, buffer);
    free(buffer);
    free(tle);
    return unpacked;
}

struct TxRequest *create_tx_request() {
    struct DopplerSettings doppler_settings = DOPPLER_SETTINGS__INIT;
    doppler_settings.altitude = 0;
    doppler_settings.n_tle = 3;
    char **tle = malloc(sizeof(char *) * doppler_settings.n_tle);
    tle[0] = "LUCKY-7";
    tle[1] = "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992";
    tle[2] = "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524";
    doppler_settings.tle = tle;
    doppler_settings.latitude = 53.72 * 10E6;
    doppler_settings.longitude = 47.57F * 10E6;
    TxRequest result = TX_REQUEST__INIT;
    result.doppler = &doppler_settings;
    result.tx_center_freq = 437525000;
    result.tx_sampling_freq = 580000;
    result.tx_dump_file = false;
    result.tx_offset = 0;
    result.mod_type = MODEM_TYPE__GMSK;
    result.mod_baud_rate = 4800;
    struct FskModulationSettings fsk_settings = FSK_MODULATION_SETTINGS__INIT;
    fsk_settings.mod_fsk_deviation = 5000;
    result.fsk_settings = &fsk_settings;
    struct FileSettings fs = FILE_SETTINGS__INIT;
    fs.filename = utils_read_and_copy_str("tx.cf32");
    fs.start_time_seconds = 0L;
    result.file_settings = &fs;

    size_t len = tx_request__get_packed_size(&result);
    uint8_t *buffer = malloc(sizeof(uint8_t) * len);
    if (buffer == NULL) {
        free(tle);
        return NULL;
    }
    tx_request__pack(&result, buffer);
    struct TxRequest *unpacked = tx_request__unpack(NULL, len, buffer);
    free(buffer);
    free(tle);
    return unpacked;
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

void assert_int16_array(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size) {
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

void assert_byte_array(const int8_t expected[], size_t expected_size, int8_t *actual, size_t actual_size) {
    ck_assert_int_eq(expected_size, actual_size);
    for (size_t i = 0; i < expected_size; i++) {
        ck_assert_int_eq(expected[i], actual[i]);
    }
}

int read_data(uint8_t *output, size_t *output_len, size_t len, FILE *file) {
    size_t left = len;

    int result = 0;
    while (left > 0) {
        size_t received = fread(output, sizeof(uint8_t), left, file);
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return -errno;
            }
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }
        if (received == 0) {
            result = -1;
            break;
        }
        left -= received;
    }
    *output_len = len - left;
    return result;
}

void assert_files(FILE *expected, size_t expected_total, uint8_t *expected_buffer, uint8_t *actual_buffer, size_t batch, FILE *actual) {
    ck_assert(expected != NULL);
    ck_assert(actual != NULL);
    size_t total_read = 0;
    while (true) {
        size_t expected_read = 0;
        int code = read_data(expected_buffer, &expected_read, batch, expected);
        if (code != 0 && expected_read == 0) {
            break;
        }
        size_t actual_read = 0;
        code = read_data(actual_buffer, &actual_read, expected_read, actual);
        if (code != 0 && actual_read == 0) {
            //the very last batch of file can return code=-1 and some partial batch
            ck_assert_int_eq(code, 0);
        }
        assert_byte_array((const int8_t *) expected_buffer, expected_read, (int8_t *) actual_buffer, actual_read);

        total_read += expected_read;
        if (expected_total != 0 && total_read > expected_total) {
            break;
        }
    }
}

char *utils_read_and_copy_str(const char *value) {
    size_t length = strlen(value);
    char *result = malloc(sizeof(char) * length + 1);
    if (result == NULL) {
        return NULL;
    }
    strncpy(result, value, length);
    result[length] = '\0';
    return result;
}

char **utils_allocate_tle(char tle[3][80]) {
    char **result = malloc(sizeof(char *) * 3);
    for (int i = 0; i < 3; i++) {
        char *cur = malloc(sizeof(char) * 80);
        strncpy(cur, tle[i], 80);
        result[i] = cur;
    }
    return result;
}