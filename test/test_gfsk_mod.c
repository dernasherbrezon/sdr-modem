#include <stdlib.h>
#include <check.h>
#include "../src/dsp/gfsk_mod.h"
#include "utils.h"
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

gfsk_mod *mod = NULL;
float *result = NULL;
uint8_t *mod_input = NULL;

void setup_byte_data(uint8_t **input, size_t input_offset, size_t len) {
    uint8_t *result = malloc(sizeof(uint8_t) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (uint8_t) (input_offset + i);
    }
    *input = result;
}

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

START_TEST(test_exceeded_input) {
    float sample_rate = 19200;
    float baud_rate = 9600;
    float deviation = 5000;
    float samples_per_symbol = sample_rate / baud_rate;
    int code = gfsk_mod_create(samples_per_symbol, (2 * M_PI * deviation / sample_rate), 0.5F, 10, &mod);
    ck_assert_int_eq(code, 0);

    setup_byte_data(&mod_input, 0, 11);

    float complex *output = NULL;
    size_t output_len = 0;
    gfsk_mod_process(mod_input, 11, &output, &output_len, mod);
    ck_assert_int_eq(output_len, 0);
}

END_TEST

START_TEST(test_normal) {
    float sample_rate = 19200;
    float baud_rate = 9600;
    float deviation = 5000;
    float samples_per_symbol = sample_rate / baud_rate;
    int code = gfsk_mod_create(samples_per_symbol, (2 * M_PI * deviation / sample_rate), 0.5F, 1000, &mod);
    ck_assert_int_eq(code, 0);

    setup_byte_data(&mod_input, 0, 10);

    float complex *output = NULL;
    size_t output_len = 0;
    gfsk_mod_process(mod_input, 10, &output, &output_len, mod);

    // 10 * 2 (samples per symbol) * 8 (bit) * 2 (complex) = 320
    const float expected[320] = {1.000000F, -0.000000F, 1.000000F, -0.000989F, 0.978427F, -0.206593F, -0.066390F, -0.997794F, -0.991445F, 0.130526F, 0.195090F, 0.980785F, 0.965926F, -0.258819F, -0.321439F, -0.946930F, -0.923880F, 0.382683F, 0.442288F, 0.896873F, 0.866025F, -0.500000F, -0.555571F,
                                 -0.831469F, -0.793353F, 0.608762F, 0.659346F, 0.751839F, 0.707106F, -0.707107F, -0.751840F, -0.659346F, -0.608762F, 0.793353F, 0.831469F, 0.555571F, 0.500001F, -0.866025F, -0.896872F, -0.442290F, -0.382686F, 0.923879F, 0.946929F, 0.321442F, 0.258820F, -0.965926F,
                                 -0.980785F, -0.195092F, -0.130528F, 0.991445F, 0.997859F, 0.065406F, 0.000003F, -1.000000F, -0.997859F, 0.065400F, 0.130522F, 0.991445F, 0.980786F, -0.195086F, -0.258814F, -0.965927F, -0.947565F, 0.319563F, -0.023484F, 0.999724F, -0.946931F, 0.321438F, -0.627222F,
                                 -0.778840F, -0.946292F, 0.323312F, 0.382685F, 0.923879F, 0.896872F, -0.442290F, -0.500001F, -0.866025F, -0.831469F, 0.555571F, 0.608761F, 0.793353F, 0.751840F, -0.659346F, -0.707106F, -0.707107F, -0.659347F, 0.751839F, 0.793352F, 0.608763F, 0.557214F, -0.830369F,
                                 -0.589968F, -0.807427F, 0.555566F, -0.831473F, 0.971739F, 0.236060F, 0.553918F, -0.832571F, -0.866028F, -0.499996F, -0.442285F, 0.896875F, 0.923881F, 0.382680F, 0.321436F, -0.946931F, -0.965927F, -0.258816F, -0.195088F, 0.980786F, 0.991445F, 0.130524F, 0.065401F,
                                 -0.997859F, -1.000000F, 0.000001F, 0.065404F, 0.997859F, 0.991445F, -0.130527F, -0.193152F, -0.981169F, -0.988106F, -0.153772F, -0.193152F, -0.981169F, 0.991445F, -0.130527F, 0.067376F, 0.997728F, -0.914638F, 0.404274F, 0.067376F, 0.997728F, 0.991445F, -0.130527F,
                                 -0.195090F, -0.980785F, -0.965926F, 0.258819F, 0.321439F, 0.946930F, 0.923880F, -0.382682F, -0.442287F, -0.896873F, -0.866026F, 0.499998F, 0.553924F, 0.832567F, 0.971736F, -0.236070F, 0.555574F, 0.831467F, -0.589959F, 0.807433F, 0.557223F, 0.830363F, 0.793346F,
                                 -0.608771F, -0.659355F, -0.751832F, -0.707099F, 0.707115F, 0.751847F, 0.659338F, 0.608753F, -0.793360F, -0.831475F, -0.555562F, -0.499992F, 0.866030F, 0.896877F, 0.442280F, 0.382675F, -0.923883F, -0.946933F, -0.321432F, -0.258812F, 0.965928F, 0.980399F, 0.197021F,
                                 0.520186F, -0.854053F, 0.980788F, 0.195076F, 0.153787F, 0.988104F, 0.980788F, 0.195076F, 0.520186F, -0.854053F, 0.980788F, 0.195076F, 0.153787F, 0.988104F, 0.981173F, 0.193130F, 0.130504F, -0.991448F, -0.997860F, -0.065381F, 0.000021F, 1.000000F, 0.997858F,
                                 -0.065424F,
                                 -0.130546F, -0.991442F, -0.980781F, 0.195110F, 0.258838F, 0.965921F, 0.947557F, -0.319586F, 0.023464F, -0.999725F, 0.947557F, -0.319586F, 0.258838F, 0.965921F, -0.980394F, 0.197048F, -0.520216F, -0.854035F, -0.980394F, 0.197048F, 0.258838F, 0.965921F, 0.946924F,
                                 -0.321458F, -0.382701F, -0.923872F, -0.896861F, 0.442312F, 0.500022F, 0.866013F, 0.831456F, -0.555591F, -0.608781F, -0.793338F, -0.751824F, 0.659364F, 0.707123F, 0.707090F, 0.660819F, -0.750546F, -0.479546F, -0.877517F, 0.660819F, -0.750546F, 0.707123F, 0.707090F,
                                 -0.751824F, 0.659364F, -0.608781F, -0.793338F, 0.830352F, -0.557239F, 0.807445F, 0.589943F, 0.830352F, -0.557239F, -0.608781F, -0.793338F, -0.751824F, 0.659364F, 0.707123F, 0.707090F, 0.659328F, -0.751855F, -0.793367F, -0.608743F, -0.557200F, 0.830378F, 0.589981F,
                                 0.807417F, -0.555552F, 0.831482F, -0.971742F, -0.236044F, -0.553901F, 0.832582F, 0.866036F, 0.499981F, 0.442270F, -0.896882F, -0.923887F, -0.382664F, -0.321420F, 0.946937F, 0.965931F, 0.258800F, 0.195071F, -0.980789F, -0.991447F, -0.130507F, -0.065385F, 0.997860F,
                                 1.000000F, -0.000018F, -0.065421F, -0.997858F, -0.991443F, 0.130543F, 0.193161F, 0.981167F, 0.988109F, 0.153755F, 0.195107F, 0.980782F, -0.854036F, 0.520213F, 0.197052F, 0.980393F, 0.965922F, -0.258835F, -0.319576F, -0.947561F};

    assert_complex_array(expected, 160, output, output_len);
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
    if (mod_input != NULL) {
        free(mod_input);
        mod_input = NULL;
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
    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_exceeded_input);

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

