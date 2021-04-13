#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp/clock_recovery_mm.h"
#include "utils.h"

clock_mm *clock = NULL;
float *float_input = NULL;

START_TEST (test_normal) {
    int code = clock_mm_create(2.0F, 0.25F * 0.175F * 0.175F, 0.005f, 0.175f, 0.005f, 100, &clock);
    ck_assert_int_eq(code, 0);

    setup_input_data(&float_input, 0, 100);
    float *output = NULL;
    size_t output_len = 0;
    clock_mm_process(float_input, 42, &output, &output_len, clock);

    const float expected[] = {3.007791F, 5.537506F, 7.992135F, 10.434598F, 12.865694F, 15.301093F, 17.738537F,
                              20.176548F, 22.611200F, 25.053421F, 27.484411F, 29.919767F, 32.351070F, 34.790939F,
                              37.227158F};
    assert_float_array(expected, sizeof(expected) / sizeof(float), output, output_len);

    clock_mm_process(float_input + 42, 38, &output, &output_len, clock);
    const float expected2[] = {39.662235F, 42.105213F, 44.534428F, 46.975555F, 49.400551F, 51.844826F, 54.276859F,
                               56.714336F, 59.148190F, 61.577019F, 64.029373F, 66.450058F, 68.900490F, 71.325760F,
                               73.759659F};
    assert_float_array(expected2, sizeof(expected2) / sizeof(float), output, output_len);

}

END_TEST

void teardown() {
    if (clock != NULL) {
        clock_mm_destroy(clock);
        clock = NULL;
    }
    if (float_input != NULL) {
        free(float_input);
        float_input = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("clock_recovery_mm");

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


