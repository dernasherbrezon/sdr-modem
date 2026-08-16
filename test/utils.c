#include "utils.h"
#include  <unity.h>
#include <volk/volk.h>
#include <errno.h>
#include <string.h>

struct RxRequest *create_rx_request() {
  struct GmskModemSettings gmsk_settings = GMSK_MODEM_SETTINGS__INIT;
  gmsk_settings.use_dc_block = true;
  gmsk_settings.transition_width = 2000;
  gmsk_settings.deviation = 5000;
  gmsk_settings.sample_rate = 48000;
  gmsk_settings.offset = 0;
  gmsk_settings.center_freq = 437525000;
  gmsk_settings.baud_rate = 4800;
  gmsk_settings.decimation = 1;
  gmsk_settings.bt = 0.5f;

  struct RxRequest result = RX_REQUEST__INIT;
  result.modem_settings_case = RX_REQUEST__MODEM_SETTINGS_GMSK;
  result.gmsk = &gmsk_settings;

  size_t len = rx_request__get_packed_size(&result);
  uint8_t *buffer = malloc(sizeof(uint8_t) * len);
  if (buffer == NULL) {
    return NULL;
  }
  rx_request__pack(&result, buffer);
  struct RxRequest *unpacked = rx_request__unpack(NULL, len, buffer);
  free(buffer);
  return unpacked;
}

struct TxRequest *create_tx_request() {
  struct GmskModemSettings gmsk_settings = GMSK_MODEM_SETTINGS__INIT;
  gmsk_settings.use_dc_block = true;
  gmsk_settings.transition_width = 2000;
  gmsk_settings.deviation = 5000;
  gmsk_settings.sample_rate = 580000;
  gmsk_settings.offset = 0;
  gmsk_settings.center_freq = 437525000;
  gmsk_settings.baud_rate = 4800;
  gmsk_settings.decimation = 1;
  gmsk_settings.bt = 0.5f;

  TxRequest result = TX_REQUEST__INIT;
  result.modem_settings_case = TX_REQUEST__MODEM_SETTINGS_GMSK;
  result.gmsk = &gmsk_settings;

  size_t len = tx_request__get_packed_size(&result);
  uint8_t *buffer = malloc(sizeof(uint8_t) * len);
  if (buffer == NULL) {
    return NULL;
  }
  tx_request__pack(&result, buffer);
  struct TxRequest *unpacked = tx_request__unpack(NULL, len, buffer);
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

void setup_volk_input_data(float **input, size_t input_offset, size_t len) {
  float *result = volk_malloc(sizeof(float) * len, volk_get_alignment());
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

void assert_files(FILE *expected, size_t expected_total, uint8_t *expected_buffer, uint8_t *actual_buffer, size_t batch, FILE *actual, int tolerance) {
  TEST_ASSERT(expected != NULL);
  TEST_ASSERT(actual != NULL);
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
    if (expected_total != 0 && total_read > expected_total) {
      break;
    }
  }
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
