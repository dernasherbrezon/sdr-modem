#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <check.h>
#include "../src/tcp_server.h"
#include "sdr_modem_client.h"
#include "utils.h"
#include "sdr_server_mock.h"
#include <stdio.h>
#include "iio_lib_mock.h"
#include "../src/api.pb-c.h"

tcp_server *server = NULL;
struct server_config *config = NULL;
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
    ck_assert_int_eq(code, 0);
}

void reconnect_client() {
    reconnect_client_with_timeout(config->read_timeout_seconds);
}

void assert_response(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details) {
    struct message_header *response_header = NULL;
    struct Response *resp = NULL;
    int code = sdr_modem_client_read_response(&response_header, &resp, client);
    ck_assert_int_eq(code, 0);
    ck_assert_int_eq(response_header->type, type);
    ck_assert_int_eq(resp->status, status);
    ck_assert_int_eq(resp->details, details);
    free(resp);
    free(response_header);
}

void assert_response_with_header_and_tx_request(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type, uint8_t type, uint8_t status, uint8_t details, struct TxRequest *req) {
    struct message_header header;
    header.protocol_version = protocol_version;
    header.type = request_type;
    int code = sdr_modem_client_write_tx_request(&header, req, client);
    ck_assert_int_eq(code, 0);

    assert_response(client, type, status, details);
}

void assert_response_with_header_and_request(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type, uint8_t type, ResponseStatus status, uint8_t details, struct RxRequest *req) {
    struct message_header header;
    header.protocol_version = protocol_version;
    header.type = request_type;
    int code = sdr_modem_client_write_request(&header, req, client);
    ck_assert_int_eq(code, 0);

    assert_response(client, type, status, details);
}

void sdr_modem_client_send_header(sdr_modem_client *client, uint8_t protocol_version, uint8_t request_type) {
    struct message_header header;
    header.protocol_version = protocol_version;
    header.type = request_type;
    header.message_length = 0;
    int code = sdr_modem_client_write_raw((uint8_t *) &header, sizeof(struct message_header), client);
    ck_assert_int_eq(code, 0);
}

void assert_response_with_request(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details, struct RxRequest *req) {
    assert_response_with_header_and_request(client, PROTOCOL_VERSION, TYPE_RX_REQUEST, type, status, details, req);
}

void assert_response_with_tx_request(sdr_modem_client *client, uint8_t type, ResponseStatus status, uint8_t details, struct TxRequest *req) {
    assert_response_with_header_and_tx_request(client, PROTOCOL_VERSION, TYPE_TX_REQUEST, type, status, details, req);
}

void init_server_with_plutosdr_support(size_t expected_tx_len) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);

    iio_lib_destroy(config->iio);
    expected_tx = malloc(sizeof(int16_t) * expected_tx_len);
    ck_assert(expected_tx != NULL);
    code = iio_lib_mock_create(NULL, 0, expected_tx, &config->iio);
    ck_assert_int_eq(code, 0);
    // speed up test a bit
    config->read_timeout_seconds = 2;
    config->tx_sdr_type = TX_SDR_TYPE_PLUTOSDR;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);

    code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
    ck_assert_int_eq(code, 0);
}

uint8_t *setup_data_to_modulate(uint32_t len) {
    data_to_modulate = malloc(sizeof(uint8_t) * len);
    ck_assert(data_to_modulate != NULL);
    for (size_t i = 0; i < len; i++) {
        data_to_modulate[i] = (uint8_t) i;
    }
    return data_to_modulate;
}

START_TEST(test_plutosdr_failures) {
    init_server_with_plutosdr_support(2048);

    // unable to initialize pluto
    config->iio->iio_create_scan_context = empty_iio_create_scan_context;
    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR, tx_req);
}

END_TEST

START_TEST(test_plutosdr_failures2) {
    init_server_with_plutosdr_support(2048);

    // init timeout a bit more for test to get ack with timeout failure
    reconnect_client_with_timeout(config->read_timeout_seconds * 2);
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    tx_req->tx_dump_file = true;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, tx_req);

    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_TX_DATA;
    struct TxData tx = TX_DATA__INIT;
    tx.data.len = 50;
    tx.data.data = setup_data_to_modulate(tx.data.len);

    //test timeout while reading tx data
    int code = sdr_modem_client_write_tx_raw(&header, &tx, tx.data.len / 2, client0);
    ck_assert_int_eq(code, 0);
    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

    //test failure to send to device
    config->iio->iio_buffer_push_partial = failing_iio_buffer_push_partial;
    code = sdr_modem_client_write_tx(&header, &tx, client0);
    ck_assert_int_eq(code, 0);
    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);

    code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, config->read_timeout_seconds, &client1);
    ck_assert_int_eq(code, 0);
    assert_response_with_tx_request(client1, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_TX_IS_BEING_USED, tx_req);

}

END_TEST

START_TEST (test_plutosdr_tx) {
    init_server_with_plutosdr_support(96000);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    tx_req->tx_dump_file = true;
    // keep test stable
    doppler_settings__free_unpacked(tx_req->doppler, NULL);
    tx_req->doppler = NULL;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, tx_req);

    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_TX_DATA;
    struct TxData tx = TX_DATA__INIT;
    tx.data.len = 50;
    tx.data.data = setup_data_to_modulate(tx.data.len);

    int code = sdr_modem_client_write_tx(&header, &tx, client0);
    ck_assert_int_eq(code, 0);
    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0);

    int16_t *actual = NULL;
    size_t actual_len = 0;
    iio_lib_mock_get_tx(&actual, &actual_len);

    const int16_t expected[] = {32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0, 32767, 0};

    // assert only first 50, thus actual_size = 50
    assert_int16_array(expected, 50, actual, 50);

    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/tx.mod2sdr.%d.cf32", config->base_path, 0);
    output_file = fopen(file_path, "rb");
    ck_assert(output_file != NULL);
    size_t buffer_len = sizeof(float complex) * config->buffer_size;
    actual_buffer = malloc(sizeof(uint8_t) * buffer_len);
    ck_assert(actual_buffer != NULL);
    size_t actual_read = 0;
    code = read_data(actual_buffer, &actual_read, sizeof(uint8_t) * buffer_len, output_file);
    ck_assert_int_eq(code, 0);

    const float expected_modulated[100] = {1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F,
                                           1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F,
                                           1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F,
                                           1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F,
                                           1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F};

    assert_complex_array(expected_modulated, 50, (float complex *) actual_buffer, 50);

}

END_TEST

START_TEST (test_invalid_config) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    free(config->bind_address);
    config->bind_address = utils_read_and_copy_str("invalid.ip");
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, -1);

    free(config->bind_address);
    // can't bind on google's ip address
    config->bind_address = utils_read_and_copy_str("142.250.187.206");
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_ping) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);
    code = sdr_modem_client_create(config->bind_address, config->port, config->buffer_size, config->read_timeout_seconds, &client0);
    ck_assert_int_eq(code, 0);
    sdr_modem_client_send_header(client0, PROTOCOL_VERSION, TYPE_PING);
    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, RESPONSE_NO_DETAILS);
}

END_TEST

START_TEST (test_invalid_requests) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    //make server timeout a bit less than client's
    //this will allow to read response for partial requests
    config->read_timeout_seconds = 2;
    config->tx_sdr_type = TX_SDR_TYPE_NONE;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);
    code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
    ck_assert_int_eq(code, 0);

    reconnect_client();
    req = create_rx_request();
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, req);
    //do not assert anything here, just make sure request are coming through
    sdr_modem_client_send_header(client0, 255, TYPE_SHUTDOWN);
    sdr_modem_client_send_header(client0, PROTOCOL_VERSION, 255);
    sdr_modem_client_send_header(client0, PROTOCOL_VERSION, TYPE_SHUTDOWN);
    sdr_modem_client_destroy_gracefully(client0);
    client0 = NULL;

    reconnect_client();
    req = create_rx_request();
    req->demod_type = 255;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->rx_center_freq = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->rx_sampling_freq = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->rx_sdr_server_band_freq = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->demod_baud_rate = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->demod_decimation = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->fsk_settings->demod_fsk_transition_width = 0;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    req = create_rx_request();
    req->demod_destination = 255;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, req);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = 255;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

    //re-create server with plutosdr support
    tcp_server_destroy(server);
    server = NULL;
    config->tx_sdr_type = TX_SDR_TYPE_PLUTOSDR;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    tx_req->tx_center_freq = 0;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    tx_req->tx_sampling_freq = 0;
    assert_response_with_tx_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST, tx_req);

    reconnect_client();
    tx_req = create_tx_request();
    tx_req->mod_type = MODEM_TYPE__GMSK;
    tx_req->mod_baud_rate = 0;
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
    ck_assert_int_eq(code, 0);
    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INVALID_REQUEST);

}

END_TEST


START_TEST(test_unable_to_connect_to_sdr_server) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    // non-existing port
    config->rx_sdr_server_port = 9999;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);

    uint32_t batch_size = 256;
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client0);
    ck_assert_int_eq(code, 0);

    req = create_rx_request();
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR, req);
}

END_TEST

START_TEST (test_multiple_clients) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    // speed up test a bit
    config->read_timeout_seconds = 2;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);
    code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
    ck_assert_int_eq(code, 0);

    uint32_t batch_size = 256;
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client0);
    ck_assert_int_eq(code, 0);

    req = create_rx_request();
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, req);

    // same freq, different baud rate
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client1);
    ck_assert_int_eq(code, 0);
    req->demod_decimation = 1;
    req->demod_baud_rate = 9600;
    assert_response_with_request(client1, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 1, req);

    // different frequency
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client2);
    ck_assert_int_eq(code, 0);
    req->rx_center_freq = 437525000 + 20000;
    assert_response_with_request(client2, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 2, req);
}

END_TEST

START_TEST (test_read_data) {
    int code = server_config_create(&config, "minimal.conf");
    ck_assert_int_eq(code, 0);
    // speed up test a bit
    config->read_timeout_seconds = 2;
    config->buffer_size = 4096;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);
    code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
    ck_assert_int_eq(code, 0);

    //connect client
    uint32_t batch_size = 256;
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, config->read_timeout_seconds, &client0);
    ck_assert_int_eq(code, 0);
    req = create_rx_request();
    // do not correct doppler - this will make test unstable and dependant on the
    // current satellite position
    doppler_settings__free_unpacked(req->doppler, NULL);
    req->doppler = NULL;
    req->rx_dump_file = true;
    req->demod_destination = DEMOD_DESTINATION__BOTH;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS__SUCCESS, 0, req);

    //send input data
    //lucky7.expected.cf32 - is already doppler-corrected data
    input_file = fopen("lucky7.expected.cf32", "rb");
    ck_assert(input_file != NULL);
    // this is important. sdr server client will wait until incoming expected_buffer is fully read
    // in real world, this is normal, but in test it will never happen (test data can be less than max expected_buffer size)
    size_t buffer_len = sizeof(float complex) * config->buffer_size;
    expected_buffer = malloc(sizeof(uint8_t) * buffer_len);
    actual_buffer = malloc(sizeof(uint8_t) * buffer_len);
    ck_assert(input_file != NULL);
    while (true) {
        size_t actual_read = 0;
        code = read_data(expected_buffer, &actual_read, buffer_len, input_file);
        if (code != 0 && actual_read == 0) {
            break;
        }
        code = sdr_server_mock_send((float complex *) expected_buffer, actual_read / sizeof(float complex), mock_server);
        ck_assert_int_eq(code, 0);
    }

    output_file = fopen("lucky7.expected.s8", "rb");
    ck_assert(output_file != NULL);
    size_t total_read = 0;
    while (true) {
        size_t actual_read = 0;
        code = read_data(expected_buffer, &actual_read, batch_size, output_file);
        if (code != 0 && actual_read == 0) {
            break;
        }
        int8_t *output = NULL;
        while (true) {
            code = sdr_modem_client_read_stream(&output, actual_read, client0);
            if (code < -1) {
                //read timeout. server is not ready to send data
                continue;
            }
            break;
        }
        ck_assert_int_eq(code, 0);
        assert_byte_array((const int8_t *) expected_buffer, actual_read, output, actual_read);
        total_read += actual_read;
        // there is not enough data in the sdr input
        // if 500 numbers matched, then I consider test passed
        // see test_fsp_demod for this number
        if (total_read > 500) {
            break;
        }
    }

    //this will trigger flush of files
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_SHUTDOWN;
    sdr_modem_client_write_request(&header, req, client0);
    sdr_modem_client_destroy_gracefully(client0);
    client0 = NULL;

    fseek(output_file, 0, SEEK_SET);
    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/rx.demod2client.%d.s8", config->base_path, 0);
    demod_file = fopen(file_path, "rb");
    assert_files(output_file, 500, expected_buffer, actual_buffer, batch_size, demod_file);

    fseek(input_file, 0, SEEK_SET);
    snprintf(file_path, sizeof(file_path), "%s/rx.sdr2demod.%d.cf32", config->base_path, 0);
    sdr_file = fopen(file_path, "rb");
    assert_files(input_file, 76000, expected_buffer, actual_buffer, batch_size, sdr_file);

}

END_TEST

void teardown() {
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
        server = NULL;
    }
    if (config != NULL) {
        server_config_destroy(config);
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

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("tcp_server");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_invalid_config);
    tcase_add_test(tc_core, test_ping);
    tcase_add_test(tc_core, test_multiple_clients);
    tcase_add_test(tc_core, test_unable_to_connect_to_sdr_server);
    tcase_add_test(tc_core, test_read_data);
    tcase_add_test(tc_core, test_invalid_requests);
    tcase_add_test(tc_core, test_plutosdr_failures);
    tcase_add_test(tc_core, test_plutosdr_failures2);
    tcase_add_test(tc_core, test_plutosdr_tx);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    // this is especially important here
    // env variable is defined in run_tests.sh, but also here
    // to run this test from IDE
    setenv("VOLK_GENERIC", "1", 1);
    setenv("VOLK_ALIGNMENT", "16", 1);

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
