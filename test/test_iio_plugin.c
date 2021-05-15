#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/sdr/iio_plugin.h"

iio_plugin *iio = NULL;

START_TEST (test_no_configs) {
    int code = iio_plugin_create(1, NULL, NULL, 10000, 2000000, &iio);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_normal) {
    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
    ck_assert(rx_config != NULL);
    rx_config->bw_hz = 2000000;
    rx_config->fs_hz = 2500000;
    rx_config->center_freq = 2500000000;
    rx_config->rfport = "A_BALANCED";
    int code = iio_plugin_create(1, rx_config, NULL, 10000, 2000000, &iio);
    ck_assert_int_eq(code, 0);
}

END_TEST

void teardown() {
    if (iio != NULL) {
        iio_plugin_destroy(iio);
        iio = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("iio_plugin");

    /* Core test case */
    tc_core = tcase_create("Core");

//    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_no_configs);

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
