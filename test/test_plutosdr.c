#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/sdr/sdr_device.h"
#include "../src/sdr/iio_lib.h"
#include "../src/sdr/plutosdr.h"
#include "iio_lib_mock.h"
#include <math.h>
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

sdr_device *sdr = NULL;
iio_lib *lib = NULL;
int16_t *expected_rx = NULL;
int16_t *expected_tx = NULL;

ssize_t invalid_iio_channel_attr_write(const struct iio_channel *chn, const char *attr, const char *src) {
    return -1;
}

struct iio_device *empty_iio_context_find_device(const struct iio_context *ctx, const char *name) {
    return NULL;
}

struct iio_channel *empty_iio_device_find_channel(const struct iio_device *dev, const char *name, bool output) {
    return NULL;
}

struct iio_buffer *empty_iio_device_create_buffer(const struct iio_device *dev, size_t samples_count, bool cyclic) {
    return NULL;
}

int invalid_iio_context_set_timeout(struct iio_context *ctx, unsigned int timeout_ms) {
    return -1;
}

struct iio_scan_context *empty_iio_create_scan_context(const char *backend, unsigned int flags) {
    return NULL;
}

ssize_t invalid_iio_scan_context_get_info_list(struct iio_scan_context *ctx, struct iio_context_info ***info) {
    return -1;
}

ssize_t empty_iio_scan_context_get_info_list(struct iio_scan_context *ctx, struct iio_context_info ***info) {
    return 0;
}

struct iio_context *empty_iio_create_context_from_uri(const char *uri) {
    return NULL;
}

void init_rx_data(size_t expected_rx_len, size_t expected_tx_len) {
    if (expected_rx_len > 0) {
        expected_rx = malloc(sizeof(int16_t) * expected_rx_len);
        ck_assert(expected_rx != NULL);
        for (size_t i = 0; i < expected_rx_len; i++) {
            expected_rx[i] = (int16_t) i;
        }
    }
    if (expected_tx_len > 0) {
        expected_tx = malloc(sizeof(int16_t) * expected_tx_len);
        ck_assert(expected_tx != NULL);
    }

    int code = iio_lib_mock_create(expected_rx, expected_rx_len, expected_tx, &lib);
    ck_assert_int_eq(code, 0);
}

struct stream_cfg *create_rx_config() {
    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
    ck_assert(rx_config != NULL);
    rx_config->sampling_freq = 528000; // (uint32_t) ((double) 25000000 / 12 + 1);
    rx_config->center_freq = 434236000;
    rx_config->gain_control_mode = IIO_GAIN_MODE_SLOW_ATTACK;
    return rx_config;
}

struct stream_cfg *create_tx_config() {
    float baud_rate = 9600;
    uint32_t sample_rate = ((int) (520834.0F / baud_rate) + 1) * baud_rate;

    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
    ck_assert(tx_config != NULL);
    tx_config->sampling_freq = sample_rate;
    tx_config->center_freq = 434236000;
    tx_config->gain_control_mode = IIO_GAIN_MODE_SLOW_ATTACK;
    return tx_config;
}

START_TEST (test_no_configs) {
    int code = iio_lib_create(&lib);
    ck_assert_int_eq(code, 0);
    code = plutosdr_create(1, false, NULL, NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_no_device) {
    int code = iio_lib_create(&lib);
    ck_assert_int_eq(code, 0);

    code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_exceeded_rx_input) {
    init_rx_data(50, 0);

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 20, lib, &sdr);
    ck_assert_int_eq(code, 0);

    float complex *actual = NULL;
    size_t actual_len = 0;
    sdr->sdr_process_rx(&actual, &actual_len, sdr->plugin);
    ck_assert_int_eq(actual_len, 0);
}

END_TEST

START_TEST (test_no_rx_config) {
    init_rx_data(50, 0);

    int code = plutosdr_create(1, false, NULL, create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, 0);

    float complex *actual = NULL;
    size_t actual_len = 0;
    sdr->sdr_process_rx(&actual, &actual_len, sdr->plugin);
    ck_assert_int_eq(actual_len, 0);
}

END_TEST

START_TEST (test_rx) {
    init_rx_data(50, 0);

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, 0);

    float complex *actual = NULL;
    size_t actual_len = 0;
    sdr->sdr_process_rx(&actual, &actual_len, sdr->plugin);

    const float expected[50] = {0.000000F, 0.000488F, 0.000977F, 0.001465F, 0.001953F, 0.002441F, 0.002930F, 0.003418F, 0.003906F, 0.004395F, 0.004883F, 0.005371F, 0.005859F, 0.006348F, 0.006836F, 0.007324F, 0.007812F, 0.008301F, 0.008789F, 0.009277F, 0.009766F, 0.010254F, 0.010742F, 0.011230F,
                                0.011719F, 0.012207F, 0.012695F, 0.013184F, 0.013672F, 0.014160F, 0.014648F, 0.015137F, 0.015625F, 0.016113F, 0.016602F, 0.017090F, 0.017578F, 0.018066F, 0.018555F, 0.019043F, 0.019531F, 0.020020F, 0.020508F, 0.020996F, 0.021484F, 0.021973F, 0.022461F, 0.022949F,
                                0.023438F, 0.023926F};
    assert_complex_array(expected, 50 / 2, actual, actual_len);
}

END_TEST

START_TEST (test_no_tx_config) {
    init_rx_data(0, 50);

    int code = plutosdr_create(1, false, create_rx_config(), NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, 0);

    float input[50] = {0.000000F, 0.000488F, 0.000977F, 0.001465F, 0.001953F, 0.002441F, 0.002930F, 0.003418F, 0.003906F, 0.004395F, 0.004883F, 0.005371F, 0.005859F, 0.006348F, 0.006836F, 0.007324F, 0.007812F, 0.008301F, 0.008789F, 0.009277F, 0.009766F, 0.010254F, 0.010742F, 0.011230F,
                       0.011719F, 0.012207F, 0.012695F, 0.013184F, 0.013672F, 0.014160F, 0.014648F, 0.015137F, 0.015625F, 0.016113F, 0.016602F, 0.017090F, 0.017578F, 0.018066F, 0.018555F, 0.019043F, 0.019531F, 0.020020F, 0.020508F, 0.020996F, 0.021484F, 0.021973F, 0.022461F, 0.022949F,
                       0.023438F, 0.023926F};
    size_t input_len = 50 / 2;
    code = sdr->sdr_process_tx((float complex *) &input, input_len, sdr->plugin);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_tx) {
    init_rx_data(0, 50);

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, 0);

    float input[50] = {0.000000F, 0.000488F, 0.000977F, 0.001465F, 0.001953F, 0.002441F, 0.002930F, 0.003418F, 0.003906F, 0.004395F, 0.004883F, 0.005371F, 0.005859F, 0.006348F, 0.006836F, 0.007324F, 0.007812F, 0.008301F, 0.008789F, 0.009277F, 0.009766F, 0.010254F, 0.010742F, 0.011230F,
                       0.011719F, 0.012207F, 0.012695F, 0.013184F, 0.013672F, 0.014160F, 0.014648F, 0.015137F, 0.015625F, 0.016113F, 0.016602F, 0.017090F, 0.017578F, 0.018066F, 0.018555F, 0.019043F, 0.019531F, 0.020020F, 0.020508F, 0.020996F, 0.021484F, 0.021973F, 0.022461F, 0.022949F,
                       0.023438F, 0.023926F};
    size_t input_len = 50 / 2;
    code = sdr->sdr_process_tx((float complex *) &input, input_len, sdr->plugin);
    ck_assert_int_eq(code, 0);

    int16_t *actual = NULL;
    size_t actual_len = 0;
    iio_lib_mock_get_tx(&actual, &actual_len);

    const int16_t expected[50] = {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256, 272, 288, 304, 320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480, 496, 512, 528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784};

    assert_int16_array(expected, 50, actual, actual_len);
}

END_TEST

START_TEST(test_invalid_scan_context) {
    init_rx_data(10, 10);
    lib->iio_create_scan_context = empty_iio_create_scan_context;

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_invalid_info_list) {
    init_rx_data(10, 10);
    lib->iio_scan_context_get_info_list = invalid_iio_scan_context_get_info_list;

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

    lib->iio_scan_context_get_info_list = empty_iio_scan_context_get_info_list;

    code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_invalid_ctx) {
    init_rx_data(10, 10);
    lib->iio_create_context_from_uri = empty_iio_create_context_from_uri;

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_invalid_settimeout) {
    init_rx_data(10, 10);
    lib->iio_context_set_timeout = invalid_iio_context_set_timeout;

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_unable_create_buffer) {
    init_rx_data(10, 10);
    lib->iio_device_create_buffer = empty_iio_device_create_buffer;

    int code = plutosdr_create(1, false, create_rx_config(), NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

    code = plutosdr_create(1, false, NULL, create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

}

END_TEST

START_TEST(test_invalid_find_channel) {
    init_rx_data(10, 10);
    lib->iio_device_find_channel = empty_iio_device_find_channel;

    int code = plutosdr_create(1, false, create_rx_config(), NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

    code = plutosdr_create(1, false, NULL, create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_unable_to_write_value) {
    init_rx_data(10, 10);
    lib->iio_channel_attr_write = invalid_iio_channel_attr_write;

    int code = plutosdr_create(1, false, create_rx_config(), create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_invalid_find_device) {
    init_rx_data(10, 10);
    lib->iio_context_find_device = empty_iio_context_find_device;

    int code = plutosdr_create(1, false, create_rx_config(), NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

    code = plutosdr_create(1, false, NULL, create_tx_config(), 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST(test_invalid_rx_config) {
    init_rx_data(10, 10);

    struct stream_cfg *rx_config = create_rx_config();
    //unknown mode
    rx_config->gain_control_mode = 255;
    int code = plutosdr_create(1, false, rx_config, NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);

    rx_config = create_rx_config();
    rx_config->sampling_freq = 100000;
    code = plutosdr_create(1, false, rx_config, NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}

END_TEST


void teardown() {
    if (sdr != NULL) {
        sdr->destroy(sdr->plugin);
        free(sdr);
        sdr = NULL;
    }
    if (lib != NULL) {
        iio_lib_destroy(lib);
        lib = NULL;
    }
    if (expected_rx != NULL) {
        free(expected_rx);
        expected_rx = NULL;
    }
    if (expected_tx != NULL) {
        free(expected_tx);
        expected_tx = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("plutosdr");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_no_device);
    tcase_add_test(tc_core, test_no_configs);
    tcase_add_test(tc_core, test_rx);
    tcase_add_test(tc_core, test_no_rx_config);
    tcase_add_test(tc_core, test_exceeded_rx_input);
    tcase_add_test(tc_core, test_tx);
    tcase_add_test(tc_core, test_no_tx_config);
    tcase_add_test(tc_core, test_invalid_scan_context);
    tcase_add_test(tc_core, test_invalid_info_list);
    tcase_add_test(tc_core, test_invalid_ctx);
    tcase_add_test(tc_core, test_invalid_settimeout);
    tcase_add_test(tc_core, test_unable_create_buffer);
    tcase_add_test(tc_core, test_invalid_find_channel);
    tcase_add_test(tc_core, test_invalid_find_device);
    tcase_add_test(tc_core, test_invalid_rx_config);
    tcase_add_test(tc_core, test_unable_to_write_value);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = common_suite();
    sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
