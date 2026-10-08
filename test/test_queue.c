#include <stdlib.h>
#include <unity.h>
#include "../src/queue.h"
#include "utils.h"

queue *queue_obj = NULL;

// expected_len is in bytes
void take_from_buffer_and_assert(const float *expected, size_t expected_len) {
  void *result = NULL;
  size_t len = 0;
  take_buffer_for_processing(&result, &len, queue_obj);
  if (expected == NULL) {
    TEST_ASSERT(result == NULL);
    return;
  }
  TEST_ASSERT(result != NULL);
  TEST_ASSERT_EQUAL_size_t(expected_len, len);
  assert_complex_array(expected, expected_len / sizeof(float complex), (float complex *) result, len / sizeof(float complex));
  queue_complete(queue_obj);
}

void test_invalid_arguments() {
  int code = create_queue(4, 0, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = create_queue(0, 10, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(-1, code);

  code = create_queue(4, 10, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(0, code);

  // this should be ignored
  TEST_ASSERT_EQUAL_INT(queue_put(NULL, 25, queue_obj), -1);

  const float buffer[2] = {1, 2};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer, 0, queue_obj), -1);

  const float buffer2[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer2, sizeof(buffer2), queue_obj), -1);

}

void test_terminated_only_after_fully_processed() {
  int code = create_queue(262144 * sizeof(float complex), 10, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  queue_put(buffer, sizeof(buffer), queue_obj);

  queue_interrupt(queue_obj);

  take_from_buffer_and_assert(buffer, sizeof(buffer));
  take_from_buffer_and_assert(NULL, 0);

  //no-op
  queue_interrupt(NULL);
}

void test_put_take() {
  int code = create_queue(262144 * sizeof(float complex), 10, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer, sizeof(buffer), queue_obj), 0);

  const float buffer2[2] = {1, 2};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer2, sizeof(buffer2), queue_obj), 0);

  take_from_buffer_and_assert(buffer, sizeof(buffer));
  take_from_buffer_and_assert(buffer2, sizeof(buffer2));
}

void test_overflow() {
  int code = create_queue(262144 * sizeof(float complex), 1, false, &queue_obj);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer, sizeof(buffer), queue_obj), 0);
  const float buffer2[10] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer2, sizeof(buffer2), queue_obj), 0);

  take_from_buffer_and_assert(buffer2, sizeof(buffer2));
}

void test_putskipped() {
  int code = create_queue(262144 * sizeof(float complex), 1, true, &queue_obj);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  TEST_ASSERT_EQUAL_INT(queue_put(buffer, sizeof(buffer), queue_obj), 0);

  queue_interrupt(queue_obj);

  // any put ignored after queue terminated
  TEST_ASSERT_EQUAL_INT(queue_put(buffer, sizeof(buffer), queue_obj), -1);
}

void tearDown() {
  destroy_queue(queue_obj);
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_put_take);
  RUN_TEST(test_overflow);
  RUN_TEST(test_terminated_only_after_fully_processed);
  RUN_TEST(test_invalid_arguments);
  RUN_TEST(test_putskipped);
  return UNITY_END();
}
