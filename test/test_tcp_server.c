#include <stdlib.h>
#include <check.h>
#include "../src/tcp_server.h"
#include "sdr_modem_client.h"
#include "utils.h"
#include "sdr_server_mock.h"
#include <stdio.h>

tcp_server *server = NULL;
struct server_config *config = NULL;
struct request *req = NULL;
sdr_modem_client *client0 = NULL;
sdr_modem_client *client1 = NULL;
sdr_modem_client *client2 = NULL;
sdr_server_mock *mock_server = NULL;

FILE *input_file = NULL;
uint8_t *buffer = NULL;

FILE *output_file = NULL;

void assert_response_with_request(sdr_modem_client *client, uint8_t type, uint8_t status, uint8_t details, struct request *req) {
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_REQUEST;
    int code = sdr_modem_client_write_request(&header, req, client);
    ck_assert_int_eq(code, 0);

    struct message_header *response_header = NULL;
    struct response *resp = NULL;
    code = sdr_modem_client_read_response(&response_header, &resp, client);
    ck_assert_int_eq(code, 0);
    ck_assert_int_eq(response_header->type, type);
    ck_assert_int_eq(resp->status, status);
    ck_assert_int_eq(resp->details, details);
    free(resp);
    free(response_header);
}

void assert_response(sdr_modem_client *client, uint8_t type, uint8_t status, uint8_t details) {
    req = create_request();
    assert_response_with_request(client, type, status, details, req);
}

START_TEST(test_unable_to_connect_to_sdr_server) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    // non-existing port
    config->rx_sdr_server_port = 9999;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);

    uint32_t batch_size = 256;
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, &client0);
    ck_assert_int_eq(code, 0);

    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_FAILURE, RESPONSE_DETAILS_INTERNAL_ERROR);
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
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, &client0);
    ck_assert_int_eq(code, 0);

    req = create_request();
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0, req);

    // same freq, different baud rate
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, &client1);
    ck_assert_int_eq(code, 0);
    req->demod_decimation = 1;
    req->demod_baud_rate = 9600;
    assert_response_with_request(client1, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 1, req);

    // different frequency
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, &client2);
    ck_assert_int_eq(code, 0);
    req->rx_center_freq = 437525000 + 20000;
    assert_response_with_request(client2, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 2, req);

}

END_TEST

START_TEST (test_read_data) {
    int code = server_config_create(&config, "minimal.conf");
    ck_assert_int_eq(code, 0);
    // speed up test a bit
    config->read_timeout_seconds = 2;
    code = tcp_server_create(config, &server);
    ck_assert_int_eq(code, 0);
    code = sdr_server_mock_create(config->rx_sdr_server_address, config->rx_sdr_server_port, &mock_response_success, config->buffer_size, &mock_server);
    ck_assert_int_eq(code, 0);

    //connect client
    uint32_t batch_size = 256;
    code = sdr_modem_client_create(config->bind_address, config->port, batch_size, &client0);
    ck_assert_int_eq(code, 0);
    req = create_request();
    req->rx_dump_file = REQUEST_DUMP_FILE_YES;
    req->demod_destination = REQUEST_DEMOD_DESTINATION_BOTH;
    assert_response_with_request(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0, req);

    //send input data
    input_file = fopen("lucky7.expected.cf32", "rb");
    ck_assert(input_file != NULL);
    buffer = malloc(sizeof(uint8_t) * config->buffer_size);
    ck_assert(input_file != NULL);
    while (true) {
        size_t actual_read = 0;
        code = read_data(buffer, &actual_read, config->buffer_size, input_file);
        if (code != 0 && actual_read == 0) {
            break;
        }
        code = sdr_server_mock_send((float complex *) buffer, actual_read / sizeof(float complex), mock_server);
        ck_assert_int_eq(code, 0);
    }

//    output_file = fopen("lucky7.expected.s8", "rb");
//    ck_assert(output_file != NULL);
//    while (true) {
//        size_t actual_read = 0;
//        int code = read_data(buffer, &actual_read, config->buffer_size, output_file);
//        if (code != 0 && actual_read == 0) {
//            break;
//        }
//        int8_t *output = NULL;
//        size_t output_len = 0;
//        code = sdr_modem_client_read_stream(&output, &output_len, actual_read, client0);
//        printf("read: %zu\n", output_len);
//        ck_assert_int_eq(code, 0);
//        //FIXME compare buffer and output
//    }

}

END_TEST

void teardown() {
    if (req != NULL) {
        free(req);
        req = NULL;
    }
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
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    if (output_file != NULL) {
        fclose(output_file);
        output_file = NULL;
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

//    tcase_add_test(tc_core, test_multiple_clients);
//    tcase_add_test(tc_core, test_unable_to_connect_to_sdr_server);
    tcase_add_test(tc_core, test_read_data);

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
