#include <stdlib.h>
#include <check.h>
#include <math.h>
#include "../src/server_config.h"

struct server_config *config = NULL;

START_TEST (test_missing_file) {
    int code = server_config_create(&config, "non-existing-configuration-file.conf");
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_invalid_format) {
    int code = server_config_create(&config, "invalid.format.conf");
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_invalid_timeout) {
    int code = server_config_create(&config, "invalid.timeout.conf");
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_unknown_tx_sdr_type) {
    int code = server_config_create(&config, "invalid.tx_sdr_type.conf");
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_minimal_config) {
    int code = server_config_create(&config, "minimal.conf");
    ck_assert_int_eq(code, 0);
}

END_TEST

START_TEST (test_success) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    ck_assert_str_eq(config->bind_address, "127.0.0.1");
    ck_assert_int_eq(config->port, 8091);
    ck_assert_int_eq(config->read_timeout_seconds, 10);
    ck_assert_int_eq(config->buffer_size, 2048);
    ck_assert_int_eq(config->rx_sdr_type, RX_SDR_TYPE_SDR_SERVER);
    ck_assert_str_eq(config->base_path, "/tmp/");
    ck_assert_int_eq(config->queue_size, 64);
    ck_assert_int_eq(config->tx_sdr_type, TX_SDR_TYPE_NONE);
    ck_assert(fabsl(0.0 - config->tx_plutosdr_gain) < 0.001);
    ck_assert_int_eq(config->tx_plutosdr_timeout_millis, 10000);
}

END_TEST

void teardown() {
    server_config_destroy(config);
    config = NULL;
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("server_config");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_success);
    tcase_add_test(tc_core, test_minimal_config);
    tcase_add_test(tc_core, test_invalid_timeout);
    tcase_add_test(tc_core, test_invalid_format);
    tcase_add_test(tc_core, test_missing_file);
    tcase_add_test(tc_core, test_unknown_tx_sdr_type);

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
