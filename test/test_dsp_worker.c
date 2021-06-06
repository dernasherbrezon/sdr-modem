#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp_worker.h"
#include "utils.h"

dsp_worker *worker = NULL;
struct server_config *config = NULL;
struct request *req = NULL;

START_TEST (test_invalid_basepath) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    const char *invalid_basepath = "/invalidpath/";
    size_t length = strlen(invalid_basepath);
    char *result = malloc(sizeof(char) * length + 1);
    ck_assert(result != NULL);
    strncpy(result, invalid_basepath, length);
    result[length] = '\0';
    config->base_path = result;
    req = create_request();
    req->rx_dump_file = REQUEST_DUMP_FILE_YES;
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, -1);

    req->rx_dump_file = REQUEST_DUMP_FILE_NO;
    req->demod_destination = REQUEST_DEMOD_DESTINATION_FILE;
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, -1);

}

END_TEST

START_TEST (test_invalid_queue_size) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    config->queue_size = 0;
    req = create_request();
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_invalid_doppler_configuration) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    req = create_request();
    char tle[3][80] = {"0", "1 0 0   0  0  00000-0  0 0  0", "2 0  0  0 0 0 0 0 0"};
    memcpy(req->tle, tle, sizeof(tle));
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_invalid_fsk_configuration) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    req = create_request();
    req->demod_baud_rate = req->rx_sampling_freq;
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_create_delete) {
    int code = server_config_create(&config, "full.conf");
    ck_assert_int_eq(code, 0);
    req = create_request();
    uint32_t id = 1;
    code = dsp_worker_create(id, 0, config, req, &worker);
    ck_assert_int_eq(code, 0);

    bool result = dsp_worker_find_by_id(&id, worker);
    ck_assert_int_eq(result, 1);
}

END_TEST

void teardown() {
    if (worker != NULL) {
        dsp_worker_destroy(worker);
        worker = NULL;
    }
    if (config != NULL) {
        server_config_destroy(config);
        config = NULL;
    }
    if (req != NULL) {
        free(req);
        req = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("dsp_worker");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_create_delete);
    tcase_add_test(tc_core, test_invalid_fsk_configuration);
    tcase_add_test(tc_core, test_invalid_doppler_configuration);
    tcase_add_test(tc_core, test_invalid_queue_size);
    tcase_add_test(tc_core, test_invalid_basepath);

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
