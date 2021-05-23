#include <stdlib.h>
#include <check.h>
#include "../src/dsp/interp_fir_filter.h"
#include "../src/dsp/gaussian_taps.h"
#include "utils.h"
#include <stdio.h>

interp_fir_filter *filter = NULL;
float *float_input = NULL;
float *taps = NULL;

START_TEST (test_normal) {
    size_t taps_len = 12;
    int code = gaussian_taps_create(1.5, 2 * (32000.0F / 1200), 0.5, 12, &taps);
    ck_assert_int_eq(code, 0);
    code = interp_fir_filter_create(taps, taps_len, 2, 1000, &filter);
    ck_assert_int_eq(code, 0);
    setup_input_data(&float_input, 0, 200);

    float *result = NULL;
    size_t result_len = 0;
    interp_fir_filter_process(float_input, 200, &result, &result_len, filter);

    const float expected[400] = {0.000000F, 0.000000F, 0.121002F, 0.123759F, 0.367951F, 0.375050F, 0.743394F, 0.755158F, 1.247330F, 1.262797F, 1.877212F, 1.894196F, 2.628097F, 2.643311F, 3.378982F, 3.392426F, 4.129867F, 4.141541F, 4.880752F, 4.890656F, 5.631638F, 5.639771F, 6.382522F, 6.388886F,
                                 7.133407F, 7.138001F, 7.884293F, 7.887116F, 8.635177F, 8.636232F, 9.386063F, 9.385346F, 10.136948F, 10.134462F, 10.887833F, 10.883576F, 11.638716F, 11.632692F, 12.389602F, 12.381806F, 13.140486F, 13.130922F, 13.891373F, 13.880036F, 14.642258F, 14.629152F, 15.393142F,
                                 15.378267F, 16.144028F, 16.127382F, 16.894911F, 16.876497F, 17.645798F, 17.625614F, 18.396681F, 18.374727F, 19.147568F, 19.123842F, 19.898451F, 19.872957F, 20.649336F, 20.622074F, 21.400221F, 21.371189F, 22.151106F, 22.120302F, 22.901991F, 22.869417F, 23.652876F,
                                 23.618534F, 24.403761F, 24.367647F, 25.154644F, 25.116762F, 25.905529F, 25.865877F, 26.656418F, 26.614994F, 27.407303F, 27.364107F, 28.158186F, 28.113222F, 28.909071F, 28.862339F, 29.659956F, 29.611454F, 30.410841F, 30.360567F, 31.161726F, 31.109682F, 31.912611F,
                                 31.858799F, 32.663494F, 32.607910F, 33.414383F, 33.357025F, 34.165268F, 34.106144F, 34.916153F, 34.855259F, 35.667034F, 35.604370F, 36.417919F, 36.353489F, 37.168808F, 37.102604F, 37.919693F, 37.851719F, 38.670574F, 38.600834F, 39.421463F, 39.349953F, 40.172348F,
                                 40.099064F, 40.923229F, 40.848179F, 41.674114F, 41.597294F, 42.424999F, 42.346409F, 43.175880F, 43.095524F, 43.926769F, 43.844639F, 44.677654F, 44.593754F, 45.428539F, 45.342873F, 46.179420F, 46.091984F, 46.930309F, 46.841103F, 47.681194F, 47.590210F, 48.432076F,
                                 48.339329F, 49.182961F, 49.088448F, 49.933846F, 49.837559F, 50.684731F, 50.586674F, 51.435619F, 51.335793F, 52.186508F, 52.084904F, 52.937393F, 52.834023F, 53.688274F, 53.583138F, 54.439159F, 54.332249F, 55.190041F, 55.081367F, 55.940929F, 55.830482F, 56.691814F,
                                 56.579597F, 57.442696F, 57.328712F, 58.193584F, 58.077827F, 58.944469F, 58.826935F, 59.695354F, 59.576057F, 60.446239F, 60.325169F, 61.197121F, 61.074287F, 61.948009F, 61.823402F, 62.698891F, 62.572517F, 63.449780F, 63.321632F, 64.200668F, 64.070747F, 64.951553F,
                                 64.819870F, 65.702431F, 65.568977F, 66.453316F, 66.318092F, 67.204208F, 67.067207F, 67.955093F, 67.816322F, 68.705978F, 68.565437F, 69.456863F, 69.314552F, 70.207741F, 70.063667F, 70.958626F, 70.812775F, 71.709511F, 71.561897F, 72.460396F, 72.311012F, 73.211288F,
                                 73.060120F, 73.962173F, 73.809242F, 74.713058F, 74.558357F, 75.463943F, 75.307472F, 76.214821F, 76.056587F, 76.965706F, 76.805710F, 77.716599F, 77.554817F, 78.467476F, 78.303932F, 79.218369F, 79.053040F, 79.969254F, 79.802162F, 80.720131F, 80.551270F, 81.471016F,
                                 81.300392F, 82.221901F, 82.049507F, 82.972786F, 82.798622F, 83.723671F, 83.547729F, 84.474556F, 84.296860F, 85.225449F, 85.045967F, 85.976334F, 85.795074F, 86.727211F, 86.544197F, 87.478096F, 87.293312F, 88.228981F, 88.042427F, 88.979866F, 88.791550F, 89.730751F,
                                 89.540657F, 90.481636F, 90.289772F, 91.232521F, 91.038879F, 91.983398F, 91.788002F, 92.734283F, 92.537117F, 93.485168F, 93.286224F, 94.236061F, 94.035347F, 94.986938F, 94.784462F, 95.737823F, 95.533577F, 96.488724F, 96.282700F, 97.239594F, 97.031807F, 97.990479F,
                                 97.780922F, 98.741364F, 98.530037F, 99.492249F, 99.279152F, 100.243134F, 100.028267F, 100.994026F, 100.777382F, 101.744904F, 101.526497F, 102.495789F, 102.275612F, 103.246689F, 103.024727F, 103.997574F, 103.773842F, 104.748459F, 104.522957F, 105.499344F, 105.272079F,
                                 106.250229F, 106.021187F, 107.001114F, 106.770302F, 107.751991F, 107.519424F, 108.502876F, 108.268532F, 109.253761F, 109.017647F, 110.004646F, 109.766769F, 110.755524F, 110.515877F, 111.506409F, 111.264992F, 112.257294F, 112.014107F, 113.008186F, 112.763229F,
                                 113.759064F, 113.512337F, 114.509949F, 114.261452F, 115.260841F, 115.010574F, 116.011726F, 115.759689F, 116.762611F, 116.508797F, 117.513496F, 117.257919F, 118.264381F, 118.007027F, 119.015266F, 118.756142F, 119.766151F, 119.505264F, 120.517036F, 120.254379F,
                                 121.267914F, 121.003487F, 122.018799F, 121.752609F, 122.769684F, 122.501724F, 123.520569F, 123.250832F, 124.271461F, 123.999954F, 125.022346F, 124.749062F, 125.773224F, 125.498177F, 126.524109F, 126.247299F, 127.274994F, 126.996414F, 128.025894F, 127.745522F,
                                 128.776779F, 128.494644F, 129.527664F, 129.243744F, 130.278549F, 129.992859F, 131.029419F, 130.741989F, 131.780304F, 131.491104F, 132.531189F, 132.240204F, 133.282074F, 132.989334F, 134.032959F, 133.738449F, 134.783844F, 134.487549F, 135.534744F, 135.236679F,
                                 136.285614F, 135.985794F, 137.036499F, 136.734894F, 137.787399F, 137.484024F, 138.538284F, 138.233139F, 139.289169F, 138.982254F, 140.040039F, 139.731369F, 140.790924F, 140.480484F, 141.541809F, 141.229584F, 142.292694F, 141.978714F, 143.043579F, 142.727829F,
                                 143.794464F, 143.476929F, 144.545349F, 144.226059F, 145.296234F, 144.975174F, 146.047119F, 145.724274F, 146.798004F, 146.473404F, 147.548904F, 147.222519F};
    assert_float_array(expected, 400, result, result_len);
}

END_TEST

void teardown() {
    if (filter != NULL) {
        interp_fir_filter_destroy(filter);
        filter = NULL;
    } else if (taps != NULL) {
        free(taps);
        taps = NULL;
    }
    if (float_input != NULL) {
        free(float_input);
        float_input = NULL;
    }
}

void setup() {
    //do nothing
}

Suite *common_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("interp_fir_filter");

    /* Core test case */
    tc_core = tcase_create("Core");

//    tcase_add_test(tc_core, test_normal);

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
