#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <stdbool.h>
#include "../src/dsp/doppler.h"
#include "utils.h"
#include "../src/sgpsdp/sgp4sdp4.h"
#include <time.h>

FILE *input_file = NULL;
uint8_t *input_buffer = NULL;
FILE *expected_file = NULL;
uint8_t *expected_buffer = NULL;
doppler *dopp = NULL;

START_TEST (test_invalid_arguments) {
    char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    int max_buffer_length = 2000;
    int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 1583840449, max_buffer_length, tle, &dopp);
    ck_assert_int_eq(code, 0);

    float complex *output = NULL;
    size_t output_len = 0;
    doppler_process(NULL, 12, &output, &output_len, dopp);
    ck_assert(output == NULL);

    const float buffer[2] = {1, 2};
    doppler_process((float complex *) buffer, 0, &output, &output_len, dopp);
    ck_assert(output == NULL);

    size_t input_buffer_len = max_buffer_length + 1;
    input_buffer = malloc(sizeof(float complex) * input_buffer_len);
    ck_assert(input_buffer != NULL);
    doppler_process((float complex *) input_buffer, input_buffer_len, &output, &output_len, dopp);
    ck_assert(output == NULL);
}

END_TEST

START_TEST (test_success) {
    char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    int max_buffer_length = 2000;
    int code = doppler_create(53.72F, 47.57F, 0.0F, 48000, 437525000, 1583840449, max_buffer_length, tle, &dopp);
    ck_assert_int_eq(code, 0);

    input_file = fopen("lucky7.cf32", "rb");
    ck_assert(input_file != NULL);
    expected_file = fopen("lucky7.expected.cf32", "rb");
    ck_assert(expected_file != NULL);

    input_buffer = malloc(max_buffer_length * sizeof(float complex));
    ck_assert(input_buffer != NULL);
    expected_buffer = malloc(max_buffer_length * sizeof(float complex));
    ck_assert(expected_buffer != NULL);
    while (true) {
        size_t actually_read = fread(input_buffer, sizeof(float complex), max_buffer_length, input_file);
        if (actually_read == 0) {
            break;
        }
        float complex *output = NULL;
        size_t output_len = 0;
        doppler_process((float complex *) input_buffer, actually_read, &output, &output_len, dopp);

        size_t actually_expected_read = fread(expected_buffer, sizeof(float complex), actually_read, expected_file);
        ck_assert_int_eq(actually_read, actually_expected_read);

        assert_complex_array((const float *) expected_buffer, actually_expected_read, output, output_len);
    }

}

END_TEST

void teardown() {
    if (dopp != NULL) {
        doppler_destroy(dopp);
        dopp = NULL;
    }
    if (input_file != NULL) {
        fclose(input_file);
        input_file = NULL;
    }
    if (input_buffer != NULL) {
        free(input_buffer);
        input_buffer = NULL;
    }
    if (expected_file != NULL) {
        fclose(expected_file);
        expected_file = NULL;
    }
    if (expected_buffer != NULL) {
        free(expected_buffer);
        expected_buffer = NULL;
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
    tcase_add_test(tc_core, test_invalid_arguments);

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
