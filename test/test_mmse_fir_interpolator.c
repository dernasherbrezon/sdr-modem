#include <check.h>
#include "../src/dsp/mmse_fir_interpolator.h"
#include "utils.h"
#include <volk/volk.h>
#include <stdio.h>

mmse_fir_interpolator *interp = NULL;
float *float_input = NULL;

START_TEST(test_normal) {
    int code = mmse_fir_interpolator_create(&interp);
    ck_assert_int_eq(code, 0);
    setup_volk_input_data(&float_input, 0, 8);
    float result = mmse_fir_interpolator_process(float_input, 0.14, interp);
    ck_assert(fabsl(3.140217F - result) < 0.001);
}

END_TEST

void teardown() {
    if (interp != NULL) {
        mmse_fir_interpolator_destroy(interp);
        interp = NULL;
    }
    if (float_input != NULL) {
        volk_free(float_input);
        float_input = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("mmse_fir_interpolator");

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