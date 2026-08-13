#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
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
  TEST_ASSERT(result != NULL);
  *result = (struct sample_struct) {0};

  result->id = id;
  result->array_len = 8;
  result->array = malloc(sizeof(uint8_t) * result->array_len);
  TEST_ASSERT(result->array != NULL);

  int code = linked_list_add(result, &sample_struct_destroy, &llist);
  TEST_ASSERT_EQUAL_INT(0, code);
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

void test_remove_by_id() {
  sample_struct_add(1);
  int id = 8;
  void *result = linked_list_remove_by_id(&id, &sample_struct_selector, &llist);
  TEST_ASSERT(result == NULL);
  id = 1;
  result = linked_list_remove_by_id(&id, &sample_struct_selector, &llist);
  TEST_ASSERT(result != NULL);
  TEST_ASSERT(llist == NULL);
  sample_struct_destroy(result);
}

void test_foreach() {
  sample_struct_add(1);
  int id = 8;
  linked_list_foreach(&id, &sample_struct_set_id, llist);

  void *result = linked_list_find(&id, &sample_struct_selector, llist);
  TEST_ASSERT(result != NULL);
  struct sample_struct *actual = (struct sample_struct *) result;
  TEST_ASSERT_EQUAL_INT(actual->id, id);
}

void test_delete_by_selector() {
  sample_struct_add(1);
  sample_struct_add(2);
  linked_list_destroy_by_selector(&sample_struct_selector_all, &llist);
  TEST_ASSERT(llist == NULL);
}

void test_delete_by_selector1() {
  sample_struct_add(1);
  sample_struct_add(2);
  linked_list_destroy_by_selector(&sample_struct_selector_one, &llist);
}

void test_delete_last2() {
  sample_struct_add(1);
  sample_struct_add(2);
  int id = 1;
  linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
  id = 2;
  linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
  TEST_ASSERT(llist == NULL);
}

void test_delete_last() {
  sample_struct_add(1);
  int id = 1;
  linked_list_destroy_by_id(&id, &sample_struct_selector, &llist);
  TEST_ASSERT(llist == NULL);
}

void test_normal() {
  sample_struct_add(1);
  sample_struct_add(2);

  int id = 2;
  void *result = linked_list_find(&id, &sample_struct_selector, llist);
  TEST_ASSERT(result != NULL);
  struct sample_struct *actual = (struct sample_struct *) result;
  TEST_ASSERT_EQUAL_INT(actual->id, id);
}

void tearDown() {
  if (llist != NULL) {
    linked_list_destroy(llist);
    llist = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_normal);
  RUN_TEST(test_delete_last);
  RUN_TEST(test_delete_last2);
  RUN_TEST(test_delete_by_selector);
  RUN_TEST(test_delete_by_selector1);
  RUN_TEST(test_foreach);
  RUN_TEST(test_remove_by_id);
  return UNITY_END();
}
