#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <unity.h>

#include "../src/tcp_server.h"
#include "../src/app_config.h"
#include "airspy_lib_mock.h"
#include "sdr_modem_client.h"

#define TEST_HOST "127.0.0.1"
#define TEST_PORT 18091
#define SDR_BUFFER_LEN 2048
// airspy is used as a server backend: it is push-based, so the test controls exactly when samples arrive
#define SDR_SAMPLE_RATE 2500000
#define SDR_FREQUENCY 437525000
#define CLIENT_TIMEOUT_SECONDS 5

app_config config;
airspy_lib *lib = NULL;
tcp_server *server = NULL;
sdr_modem_client *client = NULL;

static void start_server(void) {
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_create(&lib));
  config = (app_config) {0};
  config.bind_address = TEST_HOST;
  config.port = TEST_PORT;
  config.read_timeout_seconds = 1;
  config.sdr_buffer_len = SDR_BUFFER_LEN;
  config.queue_size = 64;
  config.max_frame_size = 1024;
  config.sdr_type = SDR_TYPE_AIRSPY;
  config.airspy_lib = lib;
  TEST_ASSERT_EQUAL_INT(0, tcp_server_create(&config, &server));
}

static void connect_client(void) {
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_create(TEST_HOST, TEST_PORT, CLIENT_TIMEOUT_SECONDS, &client));
}

static void disconnect_client(void) {
  sdr_modem_client_destroy(client);
  client = NULL;
}

static void assert_response(uint32_t request_id, response_status status, response_details details) {
  response resp;
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_wait_response(request_id, &resp, client));
  TEST_ASSERT_EQUAL_UINT8(status, resp.status);
  TEST_ASSERT_EQUAL_UINT8(details, resp.details);
}

static void assert_success(uint32_t request_id) {
  assert_response(request_id, RESPONSE_STATUS_SUCCESS, RESPONSE_DETAILS_NO_DETAILS);
}

static void assert_invalid_request(uint32_t request_id) {
  assert_response(request_id, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);
}

// the server handles requests of one client in order, so the answer to the ping means
// everything sent before was processed and nothing else is going to be sent back
static void assert_server_alive(uint32_t request_id) {
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_ping(request_id, client));
  assert_success(request_id);
}

static comm_settings gfsk_settings(void) {
  comm_settings settings = {0};
  settings.sdr_settings.frequency = SDR_FREQUENCY;
  settings.sdr_settings.sample_rate = SDR_SAMPLE_RATE;
  settings.sdr_settings.gain = 0.0f;
  settings.modem_settings.modem_type = MODEM_TYPE_GFSK;
  settings.modem_settings.modem.gfsk.sample_rate = SDR_SAMPLE_RATE;
  settings.modem_settings.modem.gfsk.baud_rate = 9600;
  settings.modem_settings.modem.gfsk.deviation = 5000;
  settings.modem_settings.modem.gfsk.bandwidth = 15600;
  settings.modem_settings.modem.gfsk.bt = 0.5f;
  settings.modem_settings.modem.gfsk.use_dc_block = 1;
  return settings;
}

static void deliver_noise(size_t buffers) {
  float complex samples[SDR_BUFFER_LEN];
  for (size_t b = 0; b < buffers; b++) {
    for (size_t i = 0; i < SDR_BUFFER_LEN; i++) {
      samples[i] = ((float) rand() / (float) RAND_MAX - 0.5f) + I * ((float) rand() / (float) RAND_MAX - 0.5f);
    }
    airspy_lib_mock_deliver(samples, SDR_BUFFER_LEN, 0, AIRSPY_LIB_SAMPLE_FLOAT32_IQ);
  }
}

// the server notices the disconnect asynchronously
static void wait_for_stop_rx(void) {
  for (int i = 0; i < 100 && airspy_lib_mock_get_stats()->stop_rx_calls == 0; i++) {
    usleep(50 * 1000);
  }
  TEST_ASSERT_GREATER_THAN_INT(0, airspy_lib_mock_get_stats()->stop_rx_calls);
}

// connection //////////////////////////////////////////////////////////////////

void test_connect_disconnect() {
  connect_client();
  disconnect_client();

  // the server accepts clients one by one. the next one must not be blocked by the first
  connect_client();
  assert_server_alive(1);
}

void test_disconnect_via_shutdown_message() {
  connect_client();
  assert_server_alive(1);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_shutdown(2, client));

  // the server closes the connection and doesn't answer
  message_header header;
  const uint8_t *payload = NULL;
  TEST_ASSERT_EQUAL_INT(-1, sdr_modem_client_receive(&header, &payload, client));
  disconnect_client();

  connect_client();
  assert_server_alive(3);
}

void test_disconnect_stops_rx() {
  comm_settings settings = gfsk_settings();
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_rx_parameters(1, &settings, client));
  assert_success(1);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_start(2, client));
  assert_success(2);
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->start_rx_calls);

  // sdr must not be left running without a client
  disconnect_client();
  wait_for_stop_rx();
}

// rx //////////////////////////////////////////////////////////////////////////

void test_rx_start_twice() {
  comm_settings settings = gfsk_settings();
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_rx_parameters(1, &settings, client));
  assert_success(1);

  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_start(2, client));
  assert_success(2);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_start(3, client));
  assert_success(3);
  // the second start is a no-op: sdr is started only once
  TEST_ASSERT_EQUAL_INT(1, airspy_lib_mock_get_stats()->start_rx_calls);

  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_stop(4, client));
  assert_success(4);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_stop(5, client));
  assert_success(5);
}

void test_rx_success() {
  comm_settings settings = gfsk_settings();
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_rx_parameters(1, &settings, client));
  assert_success(1);
  // parameters reached the sdr
  TEST_ASSERT_EQUAL_UINT32(SDR_FREQUENCY, airspy_lib_mock_get_stats()->freq);
  TEST_ASSERT_EQUAL_UINT32(SDR_SAMPLE_RATE, airspy_lib_mock_get_stats()->sample_rate);

  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_start(2, client));
  assert_success(2);

  deliver_noise(20);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_wait_soft_bits(1, client));

  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_rx_stop(3, client));
  assert_success(3);
  size_t soft_bits = sdr_modem_client_soft_bits_received(client);

  // nothing is demodulated after stop
  deliver_noise(5);
  assert_server_alive(4);
  TEST_ASSERT_EQUAL_size_t(soft_bits, sdr_modem_client_soft_bits_received(client));
}

// invalid requests ////////////////////////////////////////////////////////////

void test_unsupported_protocol_version() {
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION + 1, MESSAGE_TYPE_PING, 1, NULL, 0, client));
  assert_invalid_request(1);
  assert_server_alive(2);
}

void test_unsupported_message_type() {
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION, 99, 1, NULL, 0, client));
  assert_invalid_request(1);
  // server-to-client messages are not accepted from the client either
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_RESPONSE, 2, NULL, 0, client));
  assert_invalid_request(2);
  assert_server_alive(3);
}

void test_message_is_too_big() {
  connect_client();
  // only the header: the server must reject the message without waiting for the payload
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_RX_COMM_PARAMETERS, 1, NULL, 10 * 1024 * 1024, client));
  // the payload is never read, so the stream is out of sync: the server drops the client
  // the failure response may be lost, depending on how fast the socket is closed
  message_header header;
  const uint8_t *payload = NULL;
  int code = 0;
  while ((code = sdr_modem_client_receive(&header, &payload, client)) == 0) {
  }
  TEST_ASSERT_EQUAL_INT(-1, code);
  disconnect_client();

  connect_client();
  assert_server_alive(2);
}

void test_truncated_settings() {
  comm_settings settings = gfsk_settings();
  uint8_t buffer[256];
  size_t written = 0;
  TEST_ASSERT_EQUAL_INT(0, api_encode_comm_settings(&settings, buffer, sizeof(buffer), &written));

  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_RX_COMM_PARAMETERS, 1, buffer, written - 1, client));
  assert_invalid_request(1);
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_send(PROTOCOL_VERSION, MESSAGE_TYPE_TX_COMM_PARAMETERS, 2, buffer, written - 1, client));
  assert_invalid_request(2);
  // the rejected request must not touch the sdr
  TEST_ASSERT_EQUAL_INT(0, airspy_lib_mock_get_stats()->freq_writes);
  assert_server_alive(3);
}

void test_sdr_rejects_rx_parameters() {
  comm_settings settings = gfsk_settings();
  // airspy doesn't support such sample rate
  settings.sdr_settings.sample_rate = 123456;
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_rx_parameters(1, &settings, client));
  assert_invalid_request(1);

  // the failed request doesn't break the session
  settings = gfsk_settings();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_rx_parameters(2, &settings, client));
  assert_success(2);
}

void test_tx_is_not_supported_by_sdr() {
  comm_settings settings = gfsk_settings();
  connect_client();
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_set_tx_parameters(1, &settings, client));
  assert_invalid_request(1);
  const uint8_t frame[] = {1, 2, 3, 4};
  TEST_ASSERT_EQUAL_INT(0, sdr_modem_client_tx_frame(2, frame, sizeof(frame), client));
  assert_invalid_request(2);
  assert_server_alive(3);
}

void setUp() {
  start_server();
}

void tearDown() {
  disconnect_client();
  if (server != NULL) {
    tcp_server_shutdown(server);
    tcp_server_join_thread(server);
    server = NULL;
  }
  if (lib != NULL) {
    airspy_lib_mock_destroy(lib);
    lib = NULL;
  }
}

int main(void) {
  // the server and the client write to sockets closed by the other side
  signal(SIGPIPE, SIG_IGN);
  UNITY_BEGIN();
  RUN_TEST(test_connect_disconnect);
  RUN_TEST(test_disconnect_via_shutdown_message);
  RUN_TEST(test_disconnect_stops_rx);
  RUN_TEST(test_rx_start_twice);
  RUN_TEST(test_rx_success);
  RUN_TEST(test_unsupported_protocol_version);
  RUN_TEST(test_unsupported_message_type);
  RUN_TEST(test_message_is_too_big);
  RUN_TEST(test_truncated_settings);
  RUN_TEST(test_sdr_rejects_rx_parameters);
  RUN_TEST(test_tx_is_not_supported_by_sdr);
  return UNITY_END();
}
