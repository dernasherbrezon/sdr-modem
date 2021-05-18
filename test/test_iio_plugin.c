#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/sdr/iio_plugin.h"

iio_plugin *iio = NULL;

START_TEST (test_no_configs) {
    int code = iio_plugin_create(1, NULL, NULL, 10000, 2000000, &iio);
    ck_assert_int_eq(code, -1);
}

END_TEST

START_TEST (test_normal) {
    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
    ck_assert(rx_config != NULL);
    rx_config->sampling_freq = (uint32_t) ((double) 25000000 / 12 + 1);
    printf("sampling freq: %u\n", rx_config->sampling_freq);
    rx_config->center_freq = 434236000;
    rx_config->gain_control_mode = IIO_GAIN_MODE_SLOW_ATTACK;

    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
    ck_assert(tx_config != NULL);
    tx_config->sampling_freq = rx_config->sampling_freq;
    tx_config->center_freq = rx_config->center_freq;

    int code = iio_plugin_create(1, rx_config, tx_config, 10000, rx_config->sampling_freq, &iio);
    ck_assert_int_eq(code, 0);
//    FILE *fp = fopen("/Users/dernasherbrezon/Downloads/output.cf32", "wb");
    size_t total_len = 0;
    while (total_len < 60 * rx_config->sampling_freq) {
        float complex *output = NULL;
        size_t output_len = 0;
        iio_plugin_process_rx(&output, &output_len, iio);
//        fwrite(output, sizeof(float complex), output_len, fp);
        int code = iio_plugin_process_tx(output, output_len, iio);
        if (code < 0) {
            break;
        }
        total_len += output_len;
    }
//    fclose(fp);
}

END_TEST

void teardown() {
    if (iio != NULL) {
        iio_plugin_destroy(iio);
        iio = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("iio_plugin");

    /* Core test case */
    tc_core = tcase_create("Core");

//    tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_no_configs);

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
