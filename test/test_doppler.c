#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp/doppler.h"
#include "utils.h"
#include "../src/sgpsdp/sgp4sdp4.h"
#include <time.h>

doppler *dopp = NULL;

START_TEST (test_success) {
    char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 1583840449, 2000, tle, &dopp);
    ck_assert_int_eq(code, 0);

    float complex *input = NULL;
    size_t input_len = 0;
    float complex *output = NULL;
    size_t output_len = 0;
    doppler_process(input, input_len, &output, &output_len, dopp);
}
END_TEST

void teardown() {
    if (dopp != NULL) {
        doppler_destroy(dopp);
        dopp = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("doppler");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_success);

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
