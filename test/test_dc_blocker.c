#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp/dc_blocker.h"
#include "utils.h"

dc_blocker *dcblocker = NULL;
float *float_input = NULL;

START_TEST (test_normal) {
    int code = dc_blocker_create(32, &dcblocker);
    ck_assert_int_eq(code, 0);

    setup_input_data(&float_input, 0, 200);
    float *output = NULL;
    size_t output_len = 0;
    dc_blocker_process(float_input, 200, &output, &output_len, dcblocker);

    const float expected[] = {0.000000F,-0.000001F,-0.000006F,-0.000020F,-0.000053F,-0.000120F,-0.000240F,-0.000441F,-0.000755F,-0.001227F,-0.001909F,-0.002864F,-0.004166F,-0.005901F,-0.008171F,-0.011089F,-0.014786F,-0.019406F,-0.025114F,-0.032090F,-0.040535F,-0.050669F,-0.062733F,-0.076990F,-0.093727F,-0.113254F,-0.135904F,-0.162040F,-0.192047F,-0.226341F,-0.265366F,-0.309593F,-0.359528F,-0.415700F,-0.478666F,-0.549005F,-0.627312F,-0.714201F,-0.810299F,-0.916243F,-1.032677F,-1.160251F,-1.299616F,-1.451423F,-1.616318F,-1.794941F,-1.987923F,-2.195881F,-2.419418F,-2.659120F,-2.915548F,-3.189244F,-3.480721F,-3.790461F,-4.118916F,-4.466501F,-4.833595F,-5.220534F,-5.627611F,-6.055072F,-6.503113F,-6.971878F,-7.461456F,-6.971878F,-6.503113F,-6.055072F,-5.627611F,-5.220534F,-4.833595F,-4.466501F,-4.118916F,-3.790461F,-3.480721F,-3.189244F,-2.915548F,-2.659120F,-2.419418F,-2.195881F,-1.987923F,-1.794941F,-1.616318F,-1.451424F,-1.299618F,-1.160252F,-1.032677F,-0.916243F,-0.810299F,-0.714201F,-0.627312F,-0.549004F,-0.478664F,-0.415699F,-0.359528F,-0.309593F,-0.265366F,-0.226341F,-0.192047F,-0.162041F,-0.135906F,-0.113255F,-0.093727F,-0.076988F,-0.062729F,-0.050667F,-0.040535F,-0.032089F,-0.025112F,-0.019405F,-0.014786F,-0.011089F,-0.008171F,-0.005901F,-0.004166F,-0.002865F,-0.001911F,-0.001228F,-0.000755F,-0.000443F,-0.000244F,-0.000122F,-0.000053F,-0.000019F,-0.000004F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F,0.000000F};
    assert_float_array(expected, sizeof(expected) / sizeof(float), output, output_len);
}
END_TEST

void teardown() {
    if (dcblocker != NULL) {
        dc_blocker_destroy(dcblocker);
        dcblocker = NULL;
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

    s = suite_create("quadrature_demod");

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
