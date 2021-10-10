#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/sdr/file_source.h"
#include "utils.h"

const char *tmp_folder;
sdr_device *device = NULL;
char filename[4096];

START_TEST(test_rx_invalid_arguments) {
    int max_output_buffer_length = 2000;
    int code = file_source_create(1, "/non-existing-file", NULL, 48000, 1000, max_output_buffer_length, &device);
    ck_assert_int_eq(code, -1);

    code = file_source_create(1, filename, NULL, 48000, 1000, max_output_buffer_length, &device);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
    code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
    ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST(test_tx_invalid_arguments) {
    int max_output_buffer_length = 2000;
    int code = file_source_create(1, NULL, "/", 48000, 1000, max_output_buffer_length, &device);
    ck_assert_int_eq(code, -1);

    code = file_source_create(1, NULL, filename, 48000, 1000, max_output_buffer_length, &device);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
    code = device->sdr_process_tx((complex float *) buffer, max_output_buffer_length + 1, device->plugin);
    ck_assert_int_eq(code, -1);

    complex float *output = NULL;
    size_t output_len = 0;
    code = device->sdr_process_rx(&output, &output_len, device->plugin);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_rx_offset) {
    int code = file_source_create(1, "tx.cf32", NULL, 48000, 1000, 2000, &device);
    ck_assert_int_eq(code, 0);

    complex float *output = NULL;
    size_t output_len = 0;
    code = device->sdr_process_rx(&output, &output_len, device->plugin);
    ck_assert_int_eq(code, 0);

    const float expected[10] = {1.000000F, 2.000000F, 2.452230F, 4.357358F, 3.276715F, 7.089650F, 3.405689F, 10.069820F, 2.794229F, 13.160254F};
    size_t expected_len = sizeof(expected) / sizeof(float) / 2;
    assert_complex_array(expected, expected_len, output, output_len);
}

END_TEST

START_TEST (test_success) {
    int code = file_source_create(1, NULL, filename, 48000, 0, 2000, &device);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
    code = device->sdr_process_tx((complex float *) buffer, buffer_len, device->plugin);
    ck_assert_int_eq(code, 0);
    device->destroy(device->plugin);
    free(device);
    device = NULL;

    code = file_source_create(1, filename, NULL, 48000, 0, 2000, &device);
    ck_assert_int_eq(code, 0);
    complex float *output = NULL;
    size_t output_len = 0;
    device->sdr_process_rx(&output, &output_len, device->plugin);
    assert_complex_array(buffer, buffer_len, output, output_len);
}

END_TEST

void teardown() {
    if (device != NULL) {
        device->destroy(device->plugin);
        free(device);
        device = NULL;
    }
}

void setup() {
    tmp_folder = getenv("TMPDIR");
    if (tmp_folder == NULL) {
        tmp_folder = "/tmp";
    }
    snprintf(filename, sizeof(filename), "%s/tx.cf32", tmp_folder);
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("file_source");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_success);
    tcase_add_test(tc_core, test_rx_offset);
    tcase_add_test(tc_core, test_tx_invalid_arguments);
    tcase_add_test(tc_core, test_rx_invalid_arguments);

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
