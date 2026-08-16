#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
#include "../src/dsp_worker.h"
#include "utils.h"

dsp_worker *worker = NULL;
app_config *config = NULL;
struct RxRequest *req = NULL;

void test_invalid_queue_size() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  config->queue_size = 0;
  req = create_rx_request();
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_fsk_configuration() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  req = create_rx_request();
  req->gmsk->baud_rate = req->gmsk->sample_rate;
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_delete() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  req = create_rx_request();
  uint32_t id = 1;
  code = dsp_worker_create(id, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(0, code);

  bool result = dsp_worker_find_by_id(&id, worker);
  TEST_ASSERT_EQUAL_INT(1, result);
}

void tearDown() {
  if (worker != NULL) {
    dsp_worker_destroy(worker);
    worker = NULL;
  }
  if (config != NULL) {
    app_config_destroy(config);
    config = NULL;
  }
  if (req != NULL) {
    rx_request__free_unpacked(req, NULL);
    req = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_delete);
  RUN_TEST(test_invalid_fsk_configuration);
  RUN_TEST(test_invalid_queue_size);
  return UNITY_END();
}
