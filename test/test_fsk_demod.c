#include <stdio.h>
#include <errno.h>
#include <check.h>
#include "../src/dsp/fsk_demod.h"

fsk_demod *demod = NULL;
uint8_t *buffer = NULL;
FILE *input = NULL;
FILE *expected = NULL;

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

START_TEST(test_normal) {
    int code = fsk_demod_create(192000, 40000, 5000.0f, 1, 2000, true, 32000, &demod);
    ck_assert_int_eq(code, 0);
    FILE *input = fopen("nusat.cf32", "r");
    ck_assert(input != NULL);
    FILE *expected = fopen("processed.s8", "r");
    ck_assert(expected != NULL);
    size_t buffer_len = 2048;
    buffer = malloc(sizeof(uint8_t) * buffer_len);
    ck_assert(buffer != NULL);
    while (true) {
        size_t actual_read = 0;
        int code = read_data(buffer, &actual_read, buffer_len, input);
        if (code != 0 && actual_read == 0) {
            break;
        }
        int8_t *output = NULL;
        size_t output_len = 0;
        fsk_demod_process((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
        read_data(buffer, &actual_read, output_len, expected);
        ck_assert_int_eq(actual_read, output_len);
        for (size_t i = 0; i < actual_read; i++) {
            ck_assert_int_eq((int8_t) buffer[i], output[i]);
        }
    }
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