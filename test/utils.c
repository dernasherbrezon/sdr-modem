#include "utils.h"
#include  <unity.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

struct ModemRequest *create_request() {
  struct GfskModemSettings gfsk_settings = GFSK_MODEM_SETTINGS__INIT;
  gfsk_settings.use_dc_block = true;
  gfsk_settings.bandwidth = 15600;
  gfsk_settings.deviation = 5000;
  gfsk_settings.sample_rate = 48000;
  gfsk_settings.offset = 0;
  gfsk_settings.center_freq = 437525000;
  gfsk_settings.baud_rate = 9600;
  gfsk_settings.bt = 0.5f;

  struct ModemRequest result = MODEM_REQUEST__INIT;
  result.modem_settings_case = MODEM_REQUEST__MODEM_SETTINGS_GFSK;
  result.gfsk = &gfsk_settings;

  size_t len = modem_request__get_packed_size(&result);
  uint8_t *buffer = malloc(sizeof(uint8_t) * len);
  if (buffer == NULL) {
    return NULL;
  }
  modem_request__pack(&result, buffer);
  struct ModemRequest *unpacked = modem_request__unpack(NULL, len, buffer);
  free(buffer);
  return unpacked;
}

void setup_input_data(float **input, size_t input_offset, size_t len) {
  float *result = malloc(sizeof(float) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (float) (input_offset + i);
  }
  *input = result;
}

void setup_input_complex_data(float complex **input, size_t input_offset, size_t len) {
  float complex *result = malloc(sizeof(float complex) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    // don't care about the loss of data
    result[i] = (float) (2 * input_offset + 2 * i) + (float) (2 * input_offset + 2 * i + 1) * I;
  }
  *input = result;
}

void assert_complex_array(const float expected[], size_t expected_size, float complex *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0, j = 0; i < expected_size * 2; i += 2, j++) {
    TEST_ASSERT(fabsl(expected[i] - crealf(actual[j])) < 0.01);
    TEST_ASSERT(fabsl(expected[i + 1] - cimagf(actual[j])) < 0.01);
  }
}

void assert_int16_array(const int16_t expected[], size_t expected_size, int16_t *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size; i++) {
    TEST_ASSERT_EQUAL_INT(expected[i], actual[i]);
  }
}

void assert_float_array(const float expected[], size_t expected_size, float *actual, size_t actual_size) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size; i++) {
    TEST_ASSERT(fabsl(expected[i] - actual[i]) < 0.001);
  }
}

void assert_byte_array(const int8_t expected[], size_t expected_size, int8_t *actual, size_t actual_size, int tolerance) {
  TEST_ASSERT_EQUAL_INT(expected_size, actual_size);
  for (size_t i = 0; i < expected_size; i++) {
    TEST_ASSERT(abs((int8_t) expected[i] - actual[i]) <= tolerance);
  }
}

int read_data(uint8_t *output, size_t *output_len, size_t len, FILE *file) {
  size_t left = len;

  int result = 0;
  while (left > 0) {
    size_t received = fread(output, sizeof(uint8_t), left, file);
    if (received < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        return -errno;
      }
      if (errno == EINTR) {
        continue;
      }
      result = -1;
      break;
    }
    if (received == 0) {
      result = -1;
      break;
    }
    left -= received;
  }
  *output_len = len - left;
  return result;
}

void assert_cf32_files(const char *expected_filename, const char *actual_filename, size_t number_of_items_to_compare, float tolerance) {
  FILE *expected = fopen(expected_filename, "rb");
  TEST_ASSERT(expected != NULL);
  FILE *actual = fopen(actual_filename, "rb");
  TEST_ASSERT(actual != NULL);
  size_t batch = 1024;
  float complex *expected_buffer = malloc(sizeof(float complex) * batch);
  float complex *actual_buffer = malloc(sizeof(float complex) * batch);
  size_t total_read = 0;
  while (true) {
    size_t expected_read = 0;
    int code = read_data((uint8_t *) expected_buffer, &expected_read, sizeof(float complex) * batch, expected);
    if (code != 0 && expected_read == 0) {
      break;
    }
    size_t actual_read = 0;
    code = read_data((uint8_t *) actual_buffer, &actual_read, expected_read, actual);
    if (code != 0 && actual_read == 0) {
      //the very last batch of file can return code=-1 and some partial batch
      TEST_ASSERT_EQUAL_INT(0, code);
    }

    TEST_ASSERT_EQUAL_INT(expected_read, actual_read);
    for (size_t i = 0; i < actual_read && i < number_of_items_to_compare; i++) {
      TEST_ASSERT(fabsl(crealf(expected_buffer[i]) - crealf(actual_buffer[i])) < tolerance);
      TEST_ASSERT(fabsl(cimagf(expected_buffer[i]) - cimagf(actual_buffer[i])) < tolerance);
    }

    total_read += expected_read;
    if (number_of_items_to_compare != 0 && total_read > number_of_items_to_compare) {
      break;
    }
  }
  free(expected_buffer);
  free(actual_buffer);
  fclose(expected);
  fclose(actual);
}

void assert_s8_files(const char *expected_filename, const char *actual_filename, size_t number_of_items_to_compare, int tolerance) {
  FILE *expected = fopen(expected_filename, "rb");
  TEST_ASSERT(expected != NULL);
  FILE *actual = fopen(actual_filename, "rb");
  TEST_ASSERT(actual != NULL);
  size_t batch = 1024;
  uint8_t *expected_buffer = malloc(sizeof(uint8_t) * batch);
  uint8_t *actual_buffer = malloc(sizeof(uint8_t) * batch);
  size_t total_read = 0;
  while (true) {
    size_t expected_read = 0;
    int code = read_data(expected_buffer, &expected_read, batch, expected);
    if (code != 0 && expected_read == 0) {
      break;
    }
    size_t actual_read = 0;
    code = read_data(actual_buffer, &actual_read, expected_read, actual);
    if (code != 0 && actual_read == 0) {
      //the very last batch of file can return code=-1 and some partial batch
      TEST_ASSERT_EQUAL_INT(0, code);
    }
    assert_byte_array((const int8_t *) expected_buffer, expected_read, (int8_t *) actual_buffer, actual_read, tolerance);

    total_read += expected_read;
    if (number_of_items_to_compare != 0 && total_read > number_of_items_to_compare) {
      break;
    }
  }
  free(expected_buffer);
  free(actual_buffer);
  fclose(expected);
  fclose(actual);
}

char *utils_read_and_copy_str(const char *value) {
  size_t length = strlen(value);
  char *result = malloc(sizeof(char) * length + 1);
  if (result == NULL) {
    return NULL;
  }
  strncpy(result, value, length);
  result[length] = '\0';
  return result;
}

char **utils_allocate_tle(char tle[3][80]) {
  char **result = malloc(sizeof(char *) * 3);
  for (int i = 0; i < 3; i++) {
    char *cur = malloc(sizeof(char) * 80);
    strncpy(cur, tle[i], 80);
    result[i] = cur;
  }
  return result;
}
