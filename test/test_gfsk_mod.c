#include <stdlib.h>
#include <check.h>
#include "../src/dsp/gfsk_mod.h"
#include "utils.h"

gfsk_mod *mod = NULL;
float *result = NULL;

START_TEST (test_convolve) {
    float x[3] = {0, 1, 0.5F};
    float y[3] = {1, 2, 3};
    size_t result_len = 0;
    int code = gfsk_mod_convolve(x, 3, y, 3, &result, &result_len);
    ck_assert_int_eq(code, 0);

    const float expected[5] = {0, 1, 2.5F, 4, 1.5F};
    assert_float_array(expected, 5, result, result_len);
}

END_TEST

void teardown() {
    if (mod != NULL) {
        free(mod);
        mod = NULL;
    }
    if (result != NULL) {
        free(result);
        result = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("gfsk_mod");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_convolve);

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

