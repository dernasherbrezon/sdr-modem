#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp/quadrature_demod.h"
#include "utils.h"

quadrature_demod *quad_demod = NULL;
float complex *complex_input = NULL;

START_TEST (test_normal) {
    int code = quadrature_demod_create(25.4F, 2000, &quad_demod);
    ck_assert_int_eq(code, 0);

    setup_input_complex_data(&complex_input, 0, 200);
    float *output = NULL;
    size_t output_len = 0;
    quadrature_demod_process(complex_input, 200, &output, &output_len, quad_demod);

    const float expected[] = {0.000000F,-14.935266F,-2.203149F,-0.860684F,-0.457606F,-0.283786F,-0.193152F,-0.139943F,-0.106054F,-0.083142F,-0.066930F,-0.055038F,-0.046056F,-0.039107F,-0.033620F,-0.029212F,-0.025618F,-0.022648F,-0.020167F,-0.018072F,-0.016287F,-0.014755F,-0.013428F,-0.012273F,-0.011261F,-0.010369F,-0.009579F,-0.008876F,-0.008248F,-0.007684F,-0.007176F,-0.006717F,-0.006300F,-0.005921F,-0.005576F,-0.005259F,-0.004969F,-0.004702F,-0.004457F,-0.004229F,-0.004019F,-0.003824F,-0.003643F,-0.003475F,-0.003318F,-0.003171F,-0.003034F,-0.002906F,-0.002785F,-0.002672F,-0.002566F,-0.002466F,-0.002371F,-0.002282F,-0.002198F,-0.002119F,-0.002043F,-0.001972F,-0.001904F,-0.001840F,-0.001779F,-0.001721F,-0.001665F,-0.001613F,-0.001563F,-0.001515F,-0.001469F,-0.001425F,-0.001383F,-0.001344F,-0.001305F,-0.001269F,-0.001234F,-0.001200F,-0.001168F,-0.001136F,-0.001107F,-0.001078F,-0.001050F,-0.001024F,-0.000998F,-0.000974F,-0.000950F,-0.000927F,-0.000905F,-0.000884F,-0.000864F,-0.000844F,-0.000825F,-0.000806F,-0.000788F,-0.000771F,-0.000754F,-0.000738F,-0.000723F,-0.000707F,-0.000693F,-0.000678F,-0.000665F,-0.000651F,-0.000638F,-0.000626F,-0.000613F,-0.000601F,-0.000590F,-0.000579F,-0.000568F,-0.000557F,-0.000547F,-0.000537F,-0.000527F,-0.000518F,-0.000508F,-0.000500F,-0.000491F,-0.000482F,-0.000474F,-0.000466F,-0.000458F,-0.000450F,-0.000443F,-0.000436F,-0.000428F,-0.000421F,-0.000415F,-0.000408F,-0.000402F,-0.000395F,-0.000389F,-0.000383F,-0.000377F,-0.000371F,-0.000366F,-0.000360F,-0.000355F,-0.000350F,-0.000345F,-0.000340F,-0.000335F,-0.000330F,-0.000325F,-0.000321F,-0.000316F,-0.000312F,-0.000307F,-0.000303F,-0.000299F,-0.000295F,-0.000291F,-0.000287F,-0.000283F,-0.000279F,-0.000276F,-0.000272F,-0.000269F,-0.000265F,-0.000262F,-0.000258F,-0.000255F,-0.000252F,-0.000249F,-0.000246F,-0.000243F,-0.000240F,-0.000237F,-0.000234F,-0.000231F,-0.000228F,-0.000226F,-0.000223F,-0.000220F,-0.000218F,-0.000215F,-0.000213F,-0.000210F,-0.000208F,-0.000206F,-0.000203F,-0.000201F,-0.000199F,-0.000197F,-0.000194F,-0.000192F,-0.000190F,-0.000188F,-0.000186F,-0.000184F,-0.000182F,-0.000180F,-0.000178F,-0.000176F,-0.000175F,-0.000173F,-0.000171F,-0.000169F,-0.000167F,-0.000166F,-0.000164F,-0.000162F,-0.000161F};
    assert_float_array(expected, sizeof(expected) / sizeof(float), output, output_len);
}

END_TEST

void teardown() {
    if (quad_demod != NULL) {
        quadrature_demod_destroy(quad_demod);
        quad_demod = NULL;
    }
    if (complex_input != NULL) {
        free(complex_input);
        complex_input = NULL;
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
