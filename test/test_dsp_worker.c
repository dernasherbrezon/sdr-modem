#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp_worker.h"

dsp_worker *worker = NULL;
struct server_config *config = NULL;
struct request *req = NULL;

struct request *create_request() {
    struct request *result = malloc(sizeof(struct request));
    ck_assert(result != NULL);
    result->rx_sampling_freq = 48000;
    result->rx_sdr_server_band_freq = 437525000;
    result->rx_center_freq = 437525000;
    result->altitude = 0;
    char tle[3][80] = {"LUCKY-7", "1 44406U 19038W   20069.88080907  .00000505  00000-0  32890-4 0  9992", "2 44406  97.5270  32.5584 0026284 107.4758 252.9348 15.12089395 37524"};
    memcpy(result->tle, tle, sizeof(tle));
    result->latitude = 53.72 * 10E6;
    result->longitude = 47.57F * 10E6;
    result->correct_doppler = REQUEST_CORRECT_DOPPLER_YES;
    result->rx_destination = REQUEST_RX_DESTINATION_FILE;
    result->demod_type = REQUEST_DEMOD_TYPE_FSK;
    result->demod_fsk_use_dc_block = REQUEST_DEMOD_FSK_USE_DC_BLOCK_YES;
    result->demod_fsk_transition_width = 2000;
    result->demod_decimation = 1;
    result->demod_fsk_deviation = 5000;
    result->demod_baud_rate = 4800;
    return result;
}

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
    code = dsp_worker_create(1, 0, config, req, &worker);
    ck_assert_int_eq(code, 0);
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
