#include <stdlib.h>
#include <stdio.h>
#include <unity.h>
#include "../src/dsp_worker.h"
#include "utils.h"

dsp_worker *worker = NULL;
struct server_config *config = NULL;
struct RxRequest *req = NULL;

void test_invalid_basepath() {
  int code = server_config_create(&config, "full.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  free(config->base_path);
  config->base_path = utils_read_and_copy_str("/invalidpath/");
  req = create_rx_request();
  req->rx_dump_file = true;
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);

  req->rx_dump_file = false;
  req->demod_destination = DEMOD_DESTINATION__FILE;
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);

}

void test_invalid_queue_size() {
  int code = server_config_create(&config, "full.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  config->queue_size = 0;
  req = create_rx_request();
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_doppler_configuration() {
  int code = server_config_create(&config, "full.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  req = create_rx_request();
  for (int i = 0; i < req->doppler->n_tle; i++) {
    free(req->doppler->tle[i]);
  }
  free(req->doppler->tle);
  char tle[3][80] = {"0\0", "1 0 0   0  0  00000-0  0 0  0\0", "2 0  0  0 0 0 0 0 0\0"};
  req->doppler->tle = utils_allocate_tle(tle);
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_invalid_fsk_configuration() {
  int code = server_config_create(&config, "full.conf");
  TEST_ASSERT_EQUAL_INT(0, code);
  req = create_rx_request();
  req->demod_baud_rate = req->rx_sampling_freq;
  code = dsp_worker_create(1, 0, config, req, &worker);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_create_delete() {
  int code = server_config_create(&config, "full.conf");
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
    server_config_destroy(config);
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
  RUN_TEST(test_invalid_doppler_configuration);
  RUN_TEST(test_invalid_queue_size);
  RUN_TEST(test_invalid_basepath);
  return UNITY_END();
}
