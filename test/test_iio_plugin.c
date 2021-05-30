#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/sdr/sdr_device.h"
#include "../src/sdr/iio_lib.h"
#include "../src/sdr/iio_plugin.h"
#include "../src/dsp/gfsk_mod.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

sdr_device *sdr = NULL;
gfsk_mod *mod = NULL;
uint8_t *data = NULL;
iio_lib *lib = NULL;

void setup_byte_data(uint8_t **input, size_t input_offset, size_t len) {
    uint8_t *result = malloc(sizeof(uint8_t) * len);
    ck_assert(result != NULL);
    for (size_t i = 0; i < len; i++) {
        // don't care about the loss of data
        result[i] = (uint8_t) (input_offset + i);
    }
    *input = result;
}

START_TEST (test_no_configs) {
    int code = iio_lib_create(&lib);
    ck_assert_int_eq(code, 0);
    code = iio_plugin_create(1, NULL, NULL, 10000, 2000000, lib, &sdr);
    ck_assert_int_eq(code, -1);
}
END_TEST

START_TEST (test_normal) {
//    struct stream_cfg *rx_config = malloc(sizeof(struct stream_cfg));
//    ck_assert(rx_config != NULL);
//    rx_config->sampling_freq = 528000; // (uint32_t) ((double) 25000000 / 12 + 1);
//    printf("sampling freq: %u\n", rx_config->sampling_freq);
//    rx_config->center_freq = 434236000;
//    rx_config->gain_control_mode = IIO_GAIN_MODE_SLOW_ATTACK;
//
//    float deviation = 5000.0f;
//    float baud_rate = 9600;
//    float sample_rate = ((int) (520834.0F / baud_rate) + 1) * baud_rate;
//
//    struct stream_cfg *tx_config = malloc(sizeof(struct stream_cfg));
//    ck_assert(tx_config != NULL);
//    tx_config->sampling_freq = sample_rate;
//    tx_config->center_freq = rx_config->center_freq;
//    printf("tx sampling freq: %u\n", tx_config->sampling_freq);
//
//    int code;
//    code = iio_plugin_create(1, rx_config, tx_config, 10000, tx_config->sampling_freq, &sdr);
//    ck_assert_int_eq(code, 0);
//
//    float samples_per_symbol = sample_rate / baud_rate;
//    code = gfsk_mod_create(samples_per_symbol, (2 * M_PI * deviation / sample_rate), 0.5F, baud_rate, &mod);
//    ck_assert_int_eq(code, 0);
//
//    // 2 seconds of data
//    size_t data_len = baud_rate * 2 / 8;
//    setup_byte_data(&data, 0, data_len);
//
//    float complex *output = NULL;
//    size_t output_len = 0;
//    gfsk_mod_process(data, data_len, &output, &output_len, mod);
//    size_t remaining = output_len;
//    size_t processed = 0;
//    while (remaining > 0) {
//        size_t batch;
//        if (remaining < tx_config->sampling_freq) {
//            batch = remaining;
//        } else {
//            batch = tx_config->sampling_freq;
//        }
//        code = iio_plugin_process_tx(output + processed, batch, sdr);
//        if (code < 0) {
//            break;
//        }
//        processed += batch;
//        remaining -= batch;
//    }

//    printf("%zu\n", output_len);
//    size_t total_len = 0;
//    while (total_len < output_len) {
//        code = iio_plugin_process_tx(output + total_len, tx_config->sampling_freq, sdr);
//        ck_assert_int_eq(code, 0);
//        total_len += tx_config->sampling_freq;
//    }

//    FILE *fp = fopen("/Users/dernasherbrezon/Downloads/output.cf32", "wb");
//    fwrite(output, sizeof(float complex), output_len, fp);
//    fclose(fp);

//    iio_plugin_process_rx(&output, &output_len, sdr);
//    printf("got udpate: %zu\n", output_len);

    size_t total_len = 0;
//    while (total_len < 5 * rx_config->sampling_freq) {
//        float complex *output = NULL;
//        size_t output_len = 0;
//    code = iio_plugin_process_tx(output, output_len, sdr);
//        if (code < 0) {
//            break;
//        }
//    total_len += output_len;
//    sleep(1000);
//        code = iio_plugin_process_tx(output, output_len, sdr);
//        if (code < 0) {
//            break;
//        }
//        total_len += output_len;
//    }
//    fclose(fp);
}

END_TEST

void teardown() {
    if (lib != NULL) {
        iio_lib_destroy(lib);
        lib = NULL;
    }
    if (sdr != NULL) {
        sdr->destroy(sdr->plugin);
        sdr = NULL;
        free(sdr);
    }
    if (mod != NULL) {
        gfsk_mod_destroy(mod);
        mod = NULL;
    }
    if (data != NULL) {
        free(data);
        data = NULL;
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
