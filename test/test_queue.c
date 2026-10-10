#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <unity.h>
#include "../src/queue.h"

queue *q = NULL;

static void put_byte(uint8_t value, int expected) {
  queue_message in = {.type = value, .request_id = value, .buffer = &value, .buffer_len = 1};
  TEST_ASSERT_EQUAL_INT(expected, queue_put(&in, q));
}

static void take_expect_byte(uint8_t value) {
  queue_message out = {0};
  queue_take(&out, q);
  TEST_ASSERT_NOT_NULL(out.buffer);
  TEST_ASSERT_EQUAL_UINT8(value, *(uint8_t *) out.buffer);
  TEST_ASSERT_EQUAL_UINT8(value, out.type);
  TEST_ASSERT_EQUAL_UINT32(value, out.request_id);
  queue_complete(q);
}

void test_create_invalid_args(void) {
  TEST_ASSERT_NOT_EQUAL(0, create_queue(0, 2, true, &q));
  TEST_ASSERT_NOT_EQUAL(0, create_queue(8, 0, true, &q));
  TEST_ASSERT_NULL(q);
}

void test_put_invalid_message(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(4, 2, true, &q));
  TEST_ASSERT_EQUAL_INT(-1, queue_put(NULL, q));
  uint8_t data[8] = {0};
  queue_message too_big = {.buffer = data, .buffer_len = sizeof(data)};
  TEST_ASSERT_EQUAL_INT(-1, queue_put(&too_big, q));
  queue_message max = {.buffer = data, .buffer_len = 4};
  TEST_ASSERT_EQUAL_INT(0, queue_put(&max, q));
}

void test_put_take_fifo(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 3, true, &q));
  put_byte(1, 0);
  put_byte(2, 0);
  put_byte(3, 0);
  take_expect_byte(1);
  take_expect_byte(2);
  take_expect_byte(3);
}

void test_nonblocking_overwrites_last_when_full(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, false, &q));
  put_byte(1, 0);
  put_byte(2, 0);
  put_byte(3, 0);
  take_expect_byte(1);
  take_expect_byte(3);
  queue_message out = {0};
  queue_peak(&out, q);
  TEST_ASSERT_NULL(out.buffer);
}

static void *delayed_complete(void *arg) {
  usleep(100 * 1000);
  queue_message out = {0};
  queue_take(&out, q);
  queue_complete(q);
  return NULL;
}

void test_blocking_put_waits_for_free_node(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 1, true, &q));
  put_byte(1, 0);
  pthread_t thread;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_complete, NULL));
  // blocks until the thread frees the only node
  put_byte(2, 0);
  pthread_join(thread, NULL);
  take_expect_byte(2);
}

static void *delayed_interrupt(void *arg) {
  usleep(100 * 1000);
  queue_interrupt(q);
  return NULL;
}

void test_interrupt_wakes_take(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  pthread_t thread;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_interrupt, NULL));
  queue_message out = {0};
  out.buffer = (void *) 1;
  queue_take(&out, q);
  TEST_ASSERT_NULL(out.buffer);
  pthread_join(thread, NULL);
}

void test_interrupt_wakes_blocked_put(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 1, true, &q));
  put_byte(1, 0);
  pthread_t thread;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&thread, NULL, delayed_interrupt, NULL));
  put_byte(2, -1);
  pthread_join(thread, NULL);
}

void test_put_rejected_after_interrupt(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  queue_interrupt(q);
  put_byte(1, -1);
}

void test_take_drains_after_interrupt(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  put_byte(1, 0);
  queue_interrupt(q);
  take_expect_byte(1);
  queue_message out = {0};
  out.buffer = (void *) 1;
  queue_take(&out, q);
  TEST_ASSERT_NULL(out.buffer);
}

void test_interrupt_null(void) {
  queue_interrupt(NULL);
}

void test_peak_empty(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  queue_message message = {0};
  message.buffer = (void *) 1;
  queue_peak(&message, q);
  TEST_ASSERT_NULL(message.buffer);
}

void test_peak_available(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  uint8_t data[] = {1, 2, 3, 4};
  queue_message in = {.type = 7, .request_id = 42, .buffer = data, .buffer_len = sizeof(data)};
  TEST_ASSERT_EQUAL_INT(0, queue_put(&in, q));

  queue_message out = {0};
  queue_peak(&out, q);
  TEST_ASSERT_NOT_NULL(out.buffer);
  TEST_ASSERT_EQUAL_UINT8(7, out.type);
  TEST_ASSERT_EQUAL_UINT32(42, out.request_id);
  TEST_ASSERT_EQUAL_size_t(sizeof(data), out.buffer_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(data, out.buffer, sizeof(data));
  queue_complete(q);

  // message was consumed
  queue_peak(&out, q);
  TEST_ASSERT_NULL(out.buffer);
}

void test_peak_order_and_reuse(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, true, &q));
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t data = i;
    queue_message in = {.type = i, .request_id = i, .buffer = &data, .buffer_len = 1};
    TEST_ASSERT_EQUAL_INT(0, queue_put(&in, q));
    queue_message out = {0};
    queue_peak(&out, q);
    TEST_ASSERT_NOT_NULL(out.buffer);
    TEST_ASSERT_EQUAL_UINT8(i, *(uint8_t *) out.buffer);
    queue_complete(q);
  }
}

void test_peak_after_interrupt(void) {
  TEST_ASSERT_EQUAL_INT(0, create_queue(8, 2, false, &q));
  queue_interrupt(q);
  queue_message out = {0};
  queue_peak(&out, q);
  TEST_ASSERT_NULL(out.buffer);
}

void tearDown(void) {
  if (q != NULL) {
    destroy_queue(q);
    q = NULL;
  }
}

void setUp(void) {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_invalid_args);
  RUN_TEST(test_put_invalid_message);
  RUN_TEST(test_put_take_fifo);
  RUN_TEST(test_nonblocking_overwrites_last_when_full);
  RUN_TEST(test_blocking_put_waits_for_free_node);
  RUN_TEST(test_interrupt_wakes_take);
  RUN_TEST(test_interrupt_wakes_blocked_put);
  RUN_TEST(test_put_rejected_after_interrupt);
  RUN_TEST(test_take_drains_after_interrupt);
  RUN_TEST(test_interrupt_null);
  RUN_TEST(test_peak_empty);
  RUN_TEST(test_peak_available);
  RUN_TEST(test_peak_order_and_reuse);
  RUN_TEST(test_peak_after_interrupt);
  return UNITY_END();
}
