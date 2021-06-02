#include <stdlib.h>
#include <check.h>
#include "../src/dsp/sig_source.h"
#include "utils.h"

sig_source *source = NULL;

START_TEST (test_success) {
    int code = sig_source_create(1.0F, 4, 4, &source);
    ck_assert_int_eq(code, 0);

    float complex *output = NULL;
    size_t output_len = 0;
    sig_source_process(1, 4, &output, &output_len, source);

    const float buffer[8] = {1, 0, 0, 1, -1, 0, 0, -1};
    assert_complex_array(buffer, sizeof(buffer) / sizeof(float) / 2, output, output_len);
}

END_TEST

void teardown() {
    if (source != NULL) {
        sig_source_destroy(source);
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("sig_source");

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

