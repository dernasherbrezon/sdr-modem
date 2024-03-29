#include <stdlib.h>
#include <check.h>
#include "../src/queue.h"
#include "utils.h"

queue *queue_obj = NULL;

void take_from_buffer_and_assert(const float *expected, size_t expected_len) {
    float complex *result = NULL;
    size_t len = 0;
    take_buffer_for_processing(&result, &len, queue_obj);
    if (expected == NULL) {
        ck_assert(result == NULL);
        return;
    }
    ck_assert(result != NULL);
    assert_complex_array(expected, expected_len, result, len);
    complete_buffer_processing(queue_obj);
}

START_TEST(test_invalid_arguments) {
    int code = create_queue(4, 0, false, &queue_obj);
    ck_assert_int_eq(code, -1);

    code = create_queue(0, 10, false, &queue_obj);
    ck_assert_int_eq(code, -1);

    code = create_queue(4, 10, false, &queue_obj);
    ck_assert_int_eq(code, 0);

    // this should be ignored
    ck_assert_int_eq(-1, queue_put(NULL, 25, queue_obj));

    const float buffer[2] = {1, 2};
    ck_assert_int_eq(-1, queue_put((const float complex *) buffer, 0, queue_obj));

    const float buffer2[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t buffer2_len = sizeof(buffer2) / sizeof(float) / 2;
    ck_assert_int_eq(-1, queue_put((const float complex *) buffer2, buffer2_len, queue_obj));

}

END_TEST

START_TEST (test_terminated_only_after_fully_processed) {
    int code = create_queue(262144, 10, false, &queue_obj);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t buffer_len = sizeof(buffer) / sizeof(float) / 2;
    queue_put((const float complex *) buffer, buffer_len, queue_obj);

    interrupt_waiting_the_data(queue_obj);

    take_from_buffer_and_assert(buffer, buffer_len);
    take_from_buffer_and_assert(NULL, 0);

    //no-op
    interrupt_waiting_the_data(NULL);
}

END_TEST

START_TEST (test_put_take) {
    int code = create_queue(262144, 10, false, &queue_obj);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ck_assert_int_eq(0, queue_put((const float complex *) buffer, sizeof(buffer) / sizeof(float) / 2, queue_obj));

    const float buffer2[2] = {1, 2};
    ck_assert_int_eq(0, queue_put((const float complex *) buffer2, sizeof(buffer2) / sizeof(float) / 2, queue_obj));

    take_from_buffer_and_assert(buffer, 10 / 2);
    take_from_buffer_and_assert(buffer2, 2 / 2);
}

END_TEST

START_TEST (test_overflow) {
    int code = create_queue(262144, 1, false, &queue_obj);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ck_assert_int_eq(0, queue_put((const float complex *) buffer, sizeof(buffer) / sizeof(float) / 2, queue_obj));
    const float buffer2[10] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    ck_assert_int_eq(0, queue_put((const float complex *) buffer2, sizeof(buffer2) / sizeof(float) / 2, queue_obj));

    take_from_buffer_and_assert(buffer2, 10 / 2);
}

END_TEST

START_TEST (test_putskipped) {
    int code = create_queue(262144, 1, true, &queue_obj);
    ck_assert_int_eq(code, 0);

    const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    ck_assert_int_eq(0, queue_put((const float complex *) buffer, sizeof(buffer) / sizeof(float) / 2, queue_obj));

    interrupt_waiting_the_data(queue_obj);

    // any put ignored after queue terminated
    ck_assert_int_eq(-1, queue_put((const float complex *) buffer, sizeof(buffer) / sizeof(float) / 2, queue_obj));
}

END_TEST

void teardown() {
    destroy_queue(queue_obj);
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("queue");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_put_take);
    tcase_add_test(tc_core, test_overflow);
    tcase_add_test(tc_core, test_terminated_only_after_fully_processed);
    tcase_add_test(tc_core, test_invalid_arguments);
    tcase_add_test(tc_core, test_putskipped);

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

