#include <stdlib.h>
#include <check.h>
#include "../src/tcp_server.h"
#include "sdr_modem_client.h"
#include "utils.h"
#include "sdr_server_mock.h"

tcp_server *server = NULL;
struct server_config *config = NULL;
struct request *req = NULL;
sdr_modem_client *client0 = NULL;
sdr_server_mock *mock_server = NULL;

void assert_response(sdr_modem_client *client, uint8_t type, uint8_t status, uint8_t details) {
    struct message_header header;
    header.protocol_version = PROTOCOL_VERSION;
    header.type = TYPE_REQUEST;
    req = create_request();
    int code = sdr_modem_write_request(&header, req, client0);
    ck_assert_int_eq(code, 0);

    struct message_header *response_header = NULL;
    struct response *resp = NULL;
    code = sdr_modem_read_response(&response_header, &resp, client);
    ck_assert_int_eq(code, 0);
    ck_assert_int_eq(response_header->type, type);
    ck_assert_int_eq(resp->status, status);
    ck_assert_int_eq(resp->details, details);
    free(resp);
    free(response_header);
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

START_TEST (test_normal) {
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

    assert_response(client0, TYPE_RESPONSE, RESPONSE_STATUS_SUCCESS, 0);
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

    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_unable_to_connect_to_sdr_server);

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
