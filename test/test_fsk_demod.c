#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <check.h>
#include "../src/dsp/fsk_demod.h"

fsk_demod *demod = NULL;
uint8_t *buffer = NULL;
FILE *input = NULL;
FILE *expected = NULL;
// buffer length is important here. do not change it
// all test data was generated and verified with this buffer length
// and VOLK_GENERIC=1
// different buffer length or using SIMD can cause different precision when dealing with floats
// this small precision issue can propagate further and cause small differences in the output.
// i.e. instead of -31, it can produce -30
uint32_t max_buffer_length = 4096;

int read_data(uint8_t *output, size_t *output_len, size_t len, FILE *file) {
    size_t left = len;

    int result = 0;
    while (left > 0) {
        int received = fread(output, sizeof(uint8_t), left, file);
        if (received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return -errno;
            }
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            break;
        }
        if (received == 0) {
            result = -1;
            break;
        }
        left -= received;
    }
    *output_len = len - left;
    return result;
}

void assert_files(const char *input_filename, const char *expected_filename) {
    input = fopen(input_filename, "rb");
    ck_assert(input != NULL);
    expected = fopen(expected_filename, "rb");
    ck_assert(expected != NULL);
    size_t buffer_len = sizeof(float complex) * max_buffer_length;
    buffer = malloc(sizeof(uint8_t) * buffer_len);
    ck_assert(buffer != NULL);
    size_t j  = 0;
    while (true) {
        size_t actual_read = 0;
        int code = read_data(buffer, &actual_read, buffer_len, input);
        if (code != 0 && actual_read == 0) {
            break;
        }
        int8_t *output = NULL;
        size_t output_len = 0;
        fsk_demod_process((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
        code = read_data(buffer, &actual_read, output_len, expected);
        ck_assert_int_eq(code, 0);
        ck_assert_int_eq(actual_read, output_len);
        for (size_t i = 0; i < actual_read; i++, j++) {
            if( (int8_t) buffer[i] != output[i] ) {
                printf("failed at %zu\n", j);
            }
            ck_assert_int_eq((int8_t) buffer[i], output[i]);
        }
    }
}

START_TEST(test_normal) {
    int code = fsk_demod_create(192000, 40000, 5000, 1, 2000, true, max_buffer_length, &demod);
    ck_assert_int_eq(code, 0);
    assert_files("nusat.cf32", "processed.s8");
}

END_TEST

START_TEST(test_handle_lucky7) {
    int code = fsk_demod_create(48000, 4800, 5000, 2, 2000, true, max_buffer_length, &demod);
    ck_assert_int_eq(code, 0);
    assert_files("lucky7.expected.cf32", "lucky7.expected.s8");
}

END_TEST

START_TEST(test_no_dc) {
    int code = fsk_demod_create(48000, 4800, 5000, 2, 2000, false, max_buffer_length, &demod);
    ck_assert_int_eq(code, 0);
    assert_files("lucky7.expected.cf32", "lucky7.expected.nodc.s8");
}

END_TEST

void teardown() {
    if (demod != NULL) {
        fsk_demod_destroy(demod);
        demod = NULL;
    }
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    if (input != NULL) {
        fclose(input);
        input = NULL;
    }
    if (expected != NULL) {
        fclose(expected);
        expected = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("fsk_demod");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_handle_lucky7);
    tcase_add_test(tc_core, test_no_dc);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    // this is especially important here
    // env variable is defined in run_tests.sh, but also here
    // to run this test from IDE
    setenv("VOLK_GENERIC", "1", 1);
    setenv("VOLK_ALIGNMENT", "16", 1);

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