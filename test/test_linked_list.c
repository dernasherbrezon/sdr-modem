#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/linked_list.h"

linked_list *llist = NULL;

struct sample_struct {
    int id;
    uint8_t *array;
    size_t array_len;
};

void sample_struct_destroy(void *data) {
    struct sample_struct *test = (struct sample_struct *) data;
    if (test->array != NULL) {
        free(test->array);
    }
    free(test);
}

bool sample_struct_selector(void *arg, void *data) {
    int *id = (int *) arg;
    struct sample_struct *test = (struct sample_struct *) data;
    if (test->id == *id) {
        return true;
    }
    return false;
}

void sample_struct_add(int id) {
    struct sample_struct *result = malloc(sizeof(struct sample_struct));
    ck_assert(result != NULL);
    *result = (struct sample_struct) {0};

    result->id = id;
    result->array_len = 8;
    result->array = malloc(sizeof(uint8_t) * result->array_len);
    ck_assert(result->array != NULL);

    int code = linked_list_add(result, &sample_struct_destroy, &llist);
    ck_assert_int_eq(code, 0);
}

bool sample_struct_selector_all(void *data) {
    return true;
}

bool sample_struct_selector_one(void *data) {
    struct sample_struct *test = (struct sample_struct *) data;
    if (test->id == 1) {
        return true;
    }
    return false;
}

void sample_struct_set_id(void *arg, void *data) {
    int *param = (int *) arg;
    struct sample_struct *test = (struct sample_struct *) data;
    test->id = *param;
}

START_TEST(test_remove_by_id) {
    sample_struct_add(1);
    int id = 8;
    void *result = linked_list_remove_by_id(&id, &sample_struct_selector, &llist);
    ck_assert(result == NULL);
    id = 1;
    result = linked_list_remove_by_id(&id, &sample_struct_selector, &llist);
    ck_assert(result != NULL);
    ck_assert(llist == NULL);
    sample_struct_destroy(result);
}
END_TEST

START_TEST (test_foreach) {
    sample_struct_add(1);
    int id = 8;
    linked_list_foreach(&id, &sample_struct_set_id, llist);

    void *result = linked_list_find(&id, &sample_struct_selector, llist);
    ck_assert(result != NULL);
    struct sample_struct *actual = (struct sample_struct *) result;
    ck_assert_int_eq(id, actual->id);
}

END_TEST

START_TEST (test_delete_by_selector) {
    sample_struct_add(1);
    sample_struct_add(2);
    linked_list_destroy_by_selector(&sample_struct_selector_all, &llist);
    ck_assert(llist == NULL);
}

END_TEST

START_TEST (test_delete_by_selector1) {
    sample_struct_add(1);
    sample_struct_add(2);
    linked_list_destroy_by_selector(&sample_struct_selector_one, &llist);
}

END_TEST

START_TEST (test_delete_last2) {
    sample_struct_add(1);
    sample_struct_add(2);
    int id = 1;
    linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
    id = 2;
    linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
    ck_assert(llist == NULL);
}

END_TEST

START_TEST (test_delete_last) {
    sample_struct_add(1);
    int id = 1;
    linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
    ck_assert(llist == NULL);
}

END_TEST

START_TEST (test_normal) {
    sample_struct_add(1);
    sample_struct_add(2);

    int id = 2;
    void *result = linked_list_find(&id, &sample_struct_selector, llist);
    ck_assert(result != NULL);
    struct sample_struct *actual = (struct sample_struct *) result;
    ck_assert_int_eq(id, actual->id);
}

END_TEST

void teardown() {
    if (llist != NULL) {
        linked_list_destroy(llist);
        llist = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("linked_list");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_delete_last);
    tcase_add_test(tc_core, test_delete_last2);
    tcase_add_test(tc_core, test_delete_by_selector);
    tcase_add_test(tc_core, test_delete_by_selector1);
    tcase_add_test(tc_core, test_foreach);

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
