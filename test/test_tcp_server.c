#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <unity.h>
#include <signal.h>
#include "../src/tcp_server.h"
#include "sdr_modem_client.h"
#include "utils.h"
#include "sdr_server_mock.h"
#include <stdio.h>
#include "iio_lib_mock.h"
#include "../src/api.pb-c.h"

tcp_server *server = NULL;
app_config *config = NULL;
struct RxRequest *req = NULL;
struct TxRequest *tx_req = NULL;
sdr_modem_client *client0 = NULL;
sdr_modem_client *client1 = NULL;
sdr_modem_client *client2 = NULL;
sdr_server_mock *mock_server = NULL;

FILE *input_file = NULL;
uint8_t *expected_buffer = NULL;
uint8_t *actual_buffer = NULL;
int16_t *expected_tx = NULL;

uint8_t *data_to_modulate = NULL;

FILE *output_file = NULL;
FILE *demod_file = NULL;
FILE *sdr_file = NULL;

struct iio_scan_context *empty_iio_create_scan_context(const char *backend, unsigned int flags) {
  return NULL;
}

ssize_t failing_iio_buffer_push_partial(struct iio_buffer *buf, size_t samples_count) {
  return -1;
}

void reconnect_client_with_timeout(int read_timeout_seconds) {
  sdr_modem_client_destroy(client0);
  client0 = NULL;
  if (req != NULL) {
    rx_request__free_unpacked(req, NULL);
    req = NULL;
  }
  if (tx_req != NULL) {
    tx_request__free_unpacked(tx_req, NULL);
    tx_req = NULL;
  }
  int code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, read_timeout_seconds, &client0);
  TEST_ASSERT_EQUAL_INT(0, code);
}

void reconnect_client() {
  reconnect_client_with_timeout(config->read_timeout_seconds);
}

void assert_response(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details) {
  struct message_header *response_header = NULL;
  struct Response *resp = NULL;
  int code = sdr_modem_client_read_response(&response_header, &resp, client);
  TEST_ASSERT_EQUAL_INT(0, code);
  TEST_ASSERT_EQUAL_INT(type, response_header->type);
  TEST_ASSERT_EQUAL_INT(status, resp->status);
  TEST_ASSERT_EQUAL_INT(details, resp->details);
  free(resp);
  free(response_header);
}

void assert_response_with_header_and_tx_request(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type, uint8_t type, uint8_t status, uint8_t details, struct TxRequest *tx_req) {
  struct message_header header;
  header.protocol_version = protocol_version;
  header.type = request_type;
  int code = sdr_modem_client_write_tx_request(&header, tx_req, client);
  TEST_ASSERT_EQUAL_INT(0, code);

  assert_response(client, type, status, details);
}

void assert_response_with_header_and_request(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type, uint8_t type, ResponseStatus status, uint8_t details, struct RxRequest *rx_req) {
  struct message_header header;
  header.protocol_version = protocol_version;
  header.type = request_type;
  int code = sdr_modem_client_write_request(&header, rx_req, client);
  TEST_ASSERT_EQUAL_INT(0, code);

  assert_response(client, type, status, details);
}

void sdr_modem_client_send_header(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type) {
  struct message_header header;
  header.protocol_version = protocol_version;
  header.type = request_type;
  header.message_length = 0;
  int code = sdr_modem_client_write_raw((uint8_t *) &header, sizeof(struct message_header), client);
  TEST_ASSERT_EQUAL_INT(0, code);
}

void assert_response_with_request(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details, struct RxRequest *rx_req) {
  assert_response_with_header_and_request(client, PROTOCOL_VERSION, TYPE_RX_REQUEST, type, status, details, rx_req);
}

void assert_response_with_tx_request(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details, struct TxRequest *tx_req) {
  assert_response_with_header_and_tx_request(client, PROTOCOL_VERSION, TYPE_TX_REQUEST, type, status, details, tx_req);
}

void init_server_with_plutosdr_support(size_t expected_tx_len) {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);

  iio_lib_destroy(config->iio);
  expected_tx = malloc(sizeof(int16_t) * expected_tx_len);
  TEST_ASSERT(expected_tx != NULL);
  code = iio_lib_mock_create(NULL, 0, expected_tx, &config->iio);
  TEST_ASSERT_EQUAL_INT(0, code);
  config->tx_sdr_type = TX_SDR_TYPE_PLUTOSDR;
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);

  code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
  TEST_ASSERT_EQUAL_INT(0, code);
}

uint8_t *setup_data_to_modulate(uint32_t len) {
  data_to_modulate = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(data_to_modulate != NULL);
  for (size_t i = 0; i < len; i++) {
    data_to_modulate[i] = (uint8_t) i;
  }
  return data_to_modulate;
}

void assert_response_with_tx_data(ResponseStatus status) {
  struct message_header header;
  header.protocol_version = PROTOCOL_VERSION;
  header.type = TYPE_TX_DATA;
  struct TxData tx = TX_DATA__INIT;
  tx.data.len = 50;
  tx.data.data = setup_data_to_modulate(tx.data.len);

  int code = sdr_modem_client_write_tx(&header, &tx, client0);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_response(client0, TYPE_RESPONSE, status, 0);
}

void test_plutosdr_failures() {
  init_server_with_plutosdr_support(2048);

  // unable to initialize pluto
  config->iio->iio_create_scan_context = empty_iio_create_scan_context;
  reconnect_client();
  tx_req = create_tx_request();
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR, tx_req);
}

void test_plutosdr_failures2() {
  init_server_with_plutosdr_support(2048);

  // init timeout a bit more for test to get ack with timeout failure
  reconnect_client_with_timeout(config->read_timeout_seconds * 2);
  tx_req = create_tx_request();
  tx_req->gfsk->sample_rate = 580000;
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, tx_req);

  struct message_header header;
  header.protocol_version = PROTOCOL_VERSION;
  header.type = TYPE_TX_DATA;
  struct TxData tx = TX_DATA__INIT;
  tx.data.len = 50;
  tx.data.data = setup_data_to_modulate(tx.data.len);

  //test timeout while reading tx data
  int code = sdr_modem_client_write_tx_raw(&header, &tx, tx.data.len / 2, client0);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

  //test failure to send to device
  config->iio->iio_buffer_push_partial = failing_iio_buffer_push_partial;
  code = sdr_modem_client_write_tx(&header, &tx, client0);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);

  code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, config->read_timeout_seconds, &client1);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_response_with_tx_request(client1, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_TX_IS_BEING_USED, tx_req);
}

void test_plutosdr_tx() {
  init_server_with_plutosdr_support(96000);

  reconnect_client();
  tx_req = create_tx_request();
  tx_req->gfsk->sample_rate = 580000;
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, tx_req);
  assert_response_with_tx_data(RESPONSE_STATUS__SUCCESS);

  int16_t *actual = NULL;
  size_t actual_len = 0;
  iio_lib_mock_get_tx(&actual, &actual_len);

  const int16_t expected[] = {32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0};

  // assert only first 50, thus actual_size = 50
  assert_int16_array(expected, 50, actual, 50);
}

void test_invalid_config() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  free(config->bind_address);
  config->bind_address = utils_read_and_copy_str("invalid.ip");
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(-1, code);

  free(config->bind_address);
  // can't bind on google's ip address
  config->bind_address = utils_read_and_copy_str("142.250.187.206");
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(-1, code);
}

void test_ping() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, config->read_timeout_seconds, &client0);
  TEST_ASSERT_EQUAL_INT(0, code);
  sdr_modem_client_send_header(client0, PROTOCOL_VERSION, TYPE_PING);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, RESPONSE_NO_DETAILS);
}

void test_invalid_requests() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  //make server timeout a bit less than client's
  //this will allow to read response for partial requests
  config->read_timeout_seconds = 2;
  config->tx_sdr_type = TX_SDR_TYPE_NONE;
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
  TEST_ASSERT_EQUAL_INT(0, code);

  reconnect_client();
  req = create_rx_request();
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, req);
  //do not assert anything here, just make sure request are coming through
  sdr_modem_client_send_header(client0, 255, TYPE_SHUTDOWN);
  sdr_modem_client_send_header(client0, PROTOCOL_VERSION, 255);
  sdr_modem_client_destroy_gracefully(client0);
  client0 = NULL;

  reconnect_client();
  req = create_rx_request();
  gfsk_modem_settings__free_unpacked(req->gfsk, NULL);
  req->modem_settings_case = RX_REQUEST__MODEM_SETTINGS__NOT_SET;
  req->gfsk = NULL;
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client();
  req = create_rx_request();
  req->gfsk->center_freq = 0;
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client();
  req = create_rx_request();
  req->gfsk->sample_rate = 0;
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client();
  req = create_rx_request();
  req->gfsk->baud_rate = 0;
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client();
  req = create_rx_request();
  req->gfsk->bandwidth = 0;
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  //re-create server with plutosdr support
  tcp_server_destroy(server);
  tcp_server_join_thread(server);
  server = NULL;
  config->tx_sdr_type = TX_SDR_TYPE_PLUTOSDR;
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);

  reconnect_client();
  tx_req = create_tx_request();
  tx_req->gfsk->center_freq = 0;
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

  reconnect_client();
  tx_req = create_tx_request();
  tx_req->gfsk->sample_rate = 0;
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

  reconnect_client();
  tx_req = create_tx_request();
  tx_req->gfsk->baud_rate = 0;
  assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

  reconnect_client();
  req = create_rx_request();
  assert_response_with_header_and_request(client0, 255, TYPE_RX_REQUEST, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client();
  req = create_rx_request();
  assert_response_with_header_and_request(client0, PROTOCOL_VERSION, 255, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

  reconnect_client_with_timeout(10);
  uint8_t buffer[] = {PROTOCOL_VERSION};
  code = sdr_modem_client_write_raw(buffer, sizeof(buffer), client0);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
}

void test_unable_to_connect_to_sdr_server() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  // non-existing port
  config->rx_sdr_server_port = 9999;
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);

  uint32_t batch_size = 256;
  code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client0);
  TEST_ASSERT_EQUAL_INT(0, code);

  req = create_rx_request();
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR, req);
}

void test_multiple_clients() {
  int code = app_config_create("full.conf", &config);
  TEST_ASSERT_EQUAL_INT(0, code);
  // speed up test a bit
  config->read_timeout_seconds = 2;
  code = tcp_server_create(config, &server);
  TEST_ASSERT_EQUAL_INT(0, code);
  code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
  TEST_ASSERT_EQUAL_INT(0, code);

  uint32_t batch_size = 256;
  code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client0);
  TEST_ASSERT_EQUAL_INT(0, code);

  req = create_rx_request();
  assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, req);

  // same freq, different baud rate
  code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client1);
  TEST_ASSERT_EQUAL_INT(0, code);
  req->gfsk->baud_rate = 9600;
  assert_response_with_request(client1, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 1, req);

  // different frequency
  code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client2);
  TEST_ASSERT_EQUAL_INT(0, code);
  req->gfsk->center_freq = 437525000 + 20000;
  assert_response_with_request(client2, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 2, req);
}

void tearDown() {
  if (client0 != NULL) {
    sdr_modem_client_destroy(client0);
    client0 = NULL;
  }
  if (client1 != NULL) {
    sdr_modem_client_destroy(client1);
    client1 = NULL;
  }
  if (client2 != NULL) {
    sdr_modem_client_destroy(client2);
    client2 = NULL;
  }
  if (req != NULL) {
    rx_request__free_unpacked(req, NULL);
    req = NULL;
  }
  if (tx_req != NULL) {
    tx_request__free_unpacked(tx_req, NULL);
    tx_req = NULL;
  }
  if (server != NULL) {
    tcp_server_destroy(server);
    tcp_server_join_thread(server);
    server = NULL;
  }
  if (config != NULL) {
    app_config_destroy(config);
    config = NULL;
  }
  if (mock_server != NULL) {
    sdr_server_mock_destroy(mock_server);
    mock_server = NULL;
  }
  if (input_file != NULL) {
    fclose(input_file);
    input_file = NULL;
  }
  if (expected_buffer != NULL) {
    free(expected_buffer);
    expected_buffer = NULL;
  }
  if (actual_buffer != NULL) {
    free(actual_buffer);
    actual_buffer = NULL;
  }
  if (output_file != NULL) {
    fclose(output_file);
    output_file = NULL;
  }
  if (demod_file != NULL) {
    fclose(demod_file);
    demod_file = NULL;
  }
  if (sdr_file != NULL) {
    fclose(sdr_file);
    sdr_file = NULL;
  }
  if (data_to_modulate != NULL) {
    free(data_to_modulate);
    data_to_modulate = NULL;
  }
  if (expected_tx != NULL) {
    free(expected_tx);
    expected_tx = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_invalid_config);
  RUN_TEST(test_ping);
  RUN_TEST(test_multiple_clients);
  RUN_TEST(test_unable_to_connect_to_sdr_server);
  RUN_TEST(test_invalid_requests);
  RUN_TEST(test_plutosdr_failures);
  RUN_TEST(test_plutosdr_failures2);
  RUN_TEST(test_plutosdr_tx);
  return UNITY_END();
}
