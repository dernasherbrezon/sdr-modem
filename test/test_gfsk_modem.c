#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/gfsk_modem.h"
#include "utils.h"

gfsk_modem *demod = NULL;
uint8_t *buffer = NULL;
FILE *input = NULL;
FILE *expected = NULL;
float *result = NULL;
uint8_t *mod_input = NULL;

// buffer length is important here. do not change it
// all test data was generated and verified with this buffer length
// and VOLK_GENERIC=1
// different buffer length or using SIMD can cause different precision when dealing with floats
// this small precision issue can propagate further and cause small differences in the output.
// i.e. instead of -31, it can produce -30
uint32_t max_buffer_length = 4096;

// used to manually generate expected data
void generate_test_data(const char *input_filename, const char *output_filename) {
  input = fopen(input_filename, "rb");
  TEST_ASSERT(input != NULL);
  expected = fopen(output_filename, "wb");
  TEST_ASSERT(expected != NULL);
  size_t buffer_len = sizeof(float complex) * max_buffer_length;
  buffer = malloc(sizeof(uint8_t) * buffer_len);
  TEST_ASSERT(buffer != NULL);
  while (true) {
    size_t actual_read = 0;
    int code = read_data(buffer, &actual_read, buffer_len, input);
    if (code != 0 && actual_read == 0) {
      break;
    }
    int8_t *output = NULL;
    size_t output_len = 0;
    gfsk_modem_demodulate((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
    size_t actually_wrote = fwrite(output, sizeof(int8_t), output_len, expected);
    TEST_ASSERT_EQUAL_INT(output_len, actually_wrote);
  }
  fclose(expected);
}

void assert_files_and_demod(const char *input_filename, const char *expected_filename) {
  input = fopen(input_filename, "rb");
  TEST_ASSERT(input != NULL);
  expected = fopen(expected_filename, "rb");
  TEST_ASSERT(expected != NULL);
  size_t buffer_len = sizeof(float complex) * max_buffer_length;
  buffer = malloc(sizeof(uint8_t) * buffer_len);
  TEST_ASSERT(buffer != NULL);
  while (true) {
    size_t actual_read = 0;
    int code = read_data(buffer, &actual_read, buffer_len, input);
    if (code != 0 && actual_read == 0) {
      break;
    }
    int8_t *output = NULL;
    size_t output_len = 0;
    gfsk_modem_demodulate((const complex float *) buffer, actual_read / 8, &output, &output_len, demod);
    code = read_data(buffer, &actual_read, output_len, expected);
    TEST_ASSERT_EQUAL_INT(0, code);
    TEST_ASSERT_EQUAL_INT(output_len, actual_read);
    for (size_t i = 0; i < actual_read; i++) {
      // can't make test working across macbook, raspberrypi and travis
      // all of them have different float-precision issues
      // where results slightly different
      TEST_ASSERT(abs((int8_t) buffer[i] - output[i]) <= 2);
    }
  }
}

void setup_byte_data(uint8_t **input, size_t input_offset, size_t len) {
  uint8_t *result = malloc(sizeof(uint8_t) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (uint8_t) (input_offset + i);
  }
  *input = result;
}

void test_convolve() {
  float x[3] = {0, 1, 0.5F};
  float y[3] = {1, 2, 3};
  size_t result_len = 0;
  int code = gfsk_modem_convolve(x, 3, y, 3, &result, &result_len);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float expected[5] = {0, 1, 2.5F, 4, 1.5F};
  assert_float_array(expected, 5, result, result_len);
}

void test_exceeded_input() {
  GfskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, 10, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_byte_data(&mod_input, 0, 11);

  float complex *output = NULL;
  size_t output_len = 0;
  gfsk_modem_modulate(mod_input, 11, &output, &output_len, demod);
  TEST_ASSERT_EQUAL_INT(0, output_len);
}

void test_modulation() {
  GfskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, 1000, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);

  setup_byte_data(&mod_input, 0, 10);

  float complex *output = NULL;
  size_t output_len = 0;
  gfsk_modem_modulate(mod_input, 10, &output, &output_len, demod);

  // 10 * 2 (samples per symbol) * 8 (bit) * 2 (complex) = 320
  const float expected_output[320] = {
    1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000000F, 1.000000F, -0.000007F, 1.000000F, -0.000174F, 0.999997F, -0.002409F, 0.999803F, -0.019831F, 0.994971F, -0.100166F, 0.946348F, -0.323150F, 0.728386F, -0.685167F, 0.239612F, -0.970869F, -0.384909F, -0.922955F, -0.866113F,
    -0.499849F, -0.991444F, 0.130534F, -0.707106F, 0.707107F, -0.130525F, 0.991445F, 0.500001F, 0.866025F, 0.923880F, 0.382683F, 0.965926F, -0.258820F, 0.608761F, -0.793354F, -0.000001F, -1.000000F, -0.608762F, -0.793353F, -0.965926F, -0.258818F, -0.923879F, 0.382685F, -0.499999F, 0.866026F,
    0.130528F, 0.991445F, 0.707108F, 0.707106F, 0.991445F, 0.130524F, 0.866024F, -0.500002F, 0.382682F, -0.923880F, -0.258821F, -0.965925F, -0.793355F, -0.608760F, -1.000000F, 0.000002F, -0.793352F, 0.608763F, -0.258817F, 0.965926F, 0.382686F, 0.923879F, 0.866027F, 0.499998F, 0.991445F, -0.130529F,
    0.707105F, -0.707109F, 0.130524F, -0.991445F, -0.500003F, -0.866024F, -0.923881F, -0.382681F, -0.965925F, 0.258822F, -0.608759F, 0.793355F, 0.000003F, 1.000000F, 0.608764F, 0.793351F, 0.965927F, 0.258816F, 0.923878F, -0.382687F, 0.499997F, -0.866027F, -0.130530F, -0.991444F, -0.707110F,
    -0.707104F, -0.991445F, -0.130522F, -0.866023F, 0.500004F, -0.382680F, 0.923881F, 0.258823F, 0.965925F, 0.793356F, 0.608758F, 1.000000F, -0.000004F, 0.793351F, -0.608765F, 0.258815F, -0.965927F, -0.382687F, -0.923878F, -0.866028F, -0.499996F, -0.991444F, 0.130531F, -0.707104F, 0.707110F,
    -0.130522F, 0.991445F, 0.500004F, 0.866023F, 0.923881F, 0.382679F, 0.965925F, -0.258824F, 0.608758F, -0.793356F, -0.000005F, -1.000000F, -0.608765F, -0.793350F, -0.965927F, -0.258814F, -0.923878F, 0.382688F, -0.499996F, 0.866028F, 0.130531F, 0.991444F, 0.707110F, 0.707103F, 0.991446F, 0.130521F,
    0.866023F, -0.500004F, 0.382679F, -0.923881F, -0.258810F, -0.965928F, -0.793145F, -0.609033F, -0.999988F, -0.004813F, -0.816867F, 0.576827F, -0.446153F, 0.894957F, -0.262289F, 0.964989F, -0.445842F, 0.895112F, -0.814077F, 0.580756F, -0.999393F, 0.034845F, -0.898624F, -0.438719F, -0.795543F,
    -0.605898F, -0.898777F, -0.438407F, -0.999213F, 0.039661F, -0.790407F, 0.612582F, -0.258478F, 0.966017F, 0.382702F, 0.923872F, 0.866028F, 0.499995F, 0.991444F, -0.130532F, 0.707103F, -0.707111F, 0.130521F, -0.991446F, -0.500005F, -0.866023F, -0.923882F, -0.382678F, -0.965924F, 0.258825F,
    -0.608757F, 0.793357F, 0.000006F, 1.000000F, 0.608766F, 0.793350F, 0.965927F, 0.258813F, 0.923877F, -0.382689F, 0.499995F, -0.866028F, -0.130532F, -0.991444F, -0.707111F, -0.707103F, -0.991446F, -0.130520F, -0.866022F, 0.500005F, -0.382678F, 0.923882F, 0.258825F, 0.965924F, 0.793348F, 0.608768F,
    1.000000F, 0.000342F, 0.796274F, -0.604936F, 0.296914F, -0.954904F, -0.190859F, -0.981618F, -0.379361F, -0.925249F, -0.191200F, -0.981551F, 0.292309F, -0.956324F, 0.771659F, -0.636036F, 0.980002F, -0.198989F, 0.999994F, -0.003605F, 0.979932F, -0.199330F, 0.768585F, -0.639748F, 0.254156F,
    -0.967163F, -0.383010F, -0.923744F, -0.866036F, -0.499982F, -0.991444F, 0.130532F, -0.707102F, 0.707111F, -0.130520F, 0.991446F, 0.500005F, 0.866022F, 0.923882F, 0.382678F, 0.965924F, -0.258825F, 0.608756F, -0.793357F, -0.000006F, -1.000000F, -0.608767F, -0.793349F, -0.965928F, -0.258813F,
    -0.923877F, 0.382690F, -0.499994F, 0.866029F, 0.130533F, 0.991444F, 0.707112F, 0.707102F, 0.991446F, 0.130519F, 0.866022F, -0.500006F, 0.382677F, -0.923882F, -0.258826F, -0.965924F, -0.793358F, -0.608756F, -1.000000F, 0.000007F, -0.793349F, 0.608767F, -0.258812F, 0.965928F, 0.382690F, 0.923877F,
    0.866029F, 0.499994F, 0.991446F, -0.130519F, 0.707348F, -0.706866F, 0.135295F, -0.990805F, -0.465271F, -0.885168F, -0.829067F, -0.559150F, -0.922493F, -0.386013F, -0.829067F, -0.559150F
  };
  assert_complex_array(expected_output, 160, output, 160);
}

void test_demodulation() {
  GfskModemSettings settings = {
    .sample_rate = 192000,
    .baud_rate = 40000,
    .deviation = 5000,
    .bandwidth = 50000,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("nusat.cf32", "nusat.expected.s8");
}

void test_nan() {
  GfskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("inputnan.cf32", "nan.s8");
}

void test_handle_lucky7() {
  GfskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 4800,
    .deviation = 5000,
    .bandwidth = 5000,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.s8");
}

void test_no_dc() {
  GfskModemSettings settings = {
    .sample_rate = 48000,
    .baud_rate = 9600,
    .deviation = 5000,
    .bandwidth = 15600,
    .bt = 0.5f,
    .use_dc_block = true
  };
  int code = gfsk_modem_create(&settings, max_buffer_length, &demod);
  TEST_ASSERT_EQUAL_INT(0, code);
  assert_files_and_demod("lucky7.expected.cf32", "lucky7.expected.nodc.s8");
}

void tearDown() {
  if (demod != NULL) {
    gfsk_modem_destroy(demod);
    demod = NULL;
  }
  if (buffer != NULL) {
    free(buffer);
    buffer = NULL;
  }
  if (input != NULL) {
    fclose(input);
    input = NULL;
  }
  if (expected != NULL) {
    fclose(expected);
    expected = NULL;
  }
  if (mod_input != NULL) {
    free(mod_input);
    mod_input = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_demodulation);
  RUN_TEST(test_nan);
  // RUN_TEST(test_handle_lucky7);
  // RUN_TEST(test_no_dc);
  RUN_TEST(test_convolve);
  RUN_TEST(test_modulation);
  RUN_TEST(test_exceeded_input);
  return UNITY_END();
}
