#include <stdlib.h>
#include <check.h>
#include "../src/dsp/gaussian_taps.h"

float *taps = NULL;

START_TEST (test_normal) {
    int code = gaussian_taps_create(1.5, 2 * (48000.0F / 9600), 0.5, 12, &taps);
    ck_assert_int_eq(code, 0);

    const float expected_taps[] = {0.039070457f, 0.07415177f, 0.12205514f, 0.17424175f, 0.21572968f, 0.23164831f, 0.21572968f, 0.17424175f, 0.12205514f, 0.07415177f, 0.039070457f, 0.017854061f};

    for (int i = 0; i < 12; i++) {
        ck_assert_int_eq((int32_t) (expected_taps[i] * 10000), (int32_t) (taps[i] * 10000));
    }
}

END_TEST

void teardown() {
    if (taps != NULL) {
        free(taps);
        taps = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("gaussian_taps");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_normal);

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

