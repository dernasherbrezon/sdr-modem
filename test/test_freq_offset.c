#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <complex.h>
#include <unity.h>

#include "../src/dsp/freq_offset.h"
#include "utils.h"

const char *tmp_folder;

freq_offset *fo = NULL;
float complex *complex_input = NULL;

static void write_freq_offset_file(const char *path, const char *contents) {
  FILE *f = fopen(path, "w");
  TEST_ASSERT(f != NULL);
  fputs(contents, f);
  fclose(f);
}

static float complex *make_dc_input(size_t len) {
  float complex *result = malloc(sizeof(float complex) * len);
  TEST_ASSERT(result != NULL);
  for (size_t i = 0; i < len; i++) {
    result[i] = 1.0F + 0.0F * I;
  }
  return result;
}

// single entry at t=0 with 1hz offset and sample_rate=4hz gives a clean pi/2 rad/sample rotation
static const float dc_1hz_at_4sps_expected[] = {1.0F, 0.0F, 0.0F, 1.0F, -1.0F, 0.0F, 0.0F, -1.0F};

void test_flat_before_first_entry_file_mode() {
  char offset_filename[4096];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_single_entry.txt", tmp_folder);
  write_freq_offset_file(offset_filename, "0 1\n");
  int code = freq_offset_create(offset_filename, 4, 16, &fo);
  TEST_ASSERT_EQUAL_INT(0, code);

  complex_input = make_dc_input(4);
  float complex *output = NULL;
  size_t output_len = 0;
  freq_offset_process(complex_input, 4, &output, &output_len, fo);

  assert_complex_array(dc_1hz_at_4sps_expected, 4, output, output_len);
}

void test_negative_offset_rotates_the_other_way() {
  char offset_filename[PATH_MAX];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_negative_entry.txt", tmp_folder);
  write_freq_offset_file(offset_filename, "0 -1\n");
  int code = freq_offset_create(offset_filename, 4, 16, &fo);
  TEST_ASSERT_EQUAL_INT(0, code);

  complex_input = make_dc_input(4);
  float complex *output = NULL;
  size_t output_len = 0;
  freq_offset_process(complex_input, 4, &output, &output_len, fo);

  // mirror image of dc_1hz_at_4sps_expected: same magnitude, opposite rotation direction
  const float expected[] = {1.0F, 0.0F, 0.0F, -1.0F, -1.0F, 0.0F, 0.0F, 1.0F};
  assert_complex_array(expected, 4, output, output_len);
}

void test_flat_before_first_entry_realtime_future() {
  double future_ts = (double) time(NULL) + 1000000.0;
  char content[256];
  snprintf(content, sizeof(content), "%.6f 1\n%.6f 2\n", future_ts, future_ts + 10.0);
  char offset_filename[PATH_MAX];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_future.txt", tmp_folder);
  write_freq_offset_file(offset_filename, content);

  int code = freq_offset_create(offset_filename, 4, 16, &fo);
  TEST_ASSERT_EQUAL_INT(0, code);

  complex_input = make_dc_input(4);
  float complex *output = NULL;
  size_t output_len = 0;
  freq_offset_process(complex_input, 4, &output, &output_len, fo);

  assert_complex_array(dc_1hz_at_4sps_expected, 4, output, output_len);
}

void test_all_entries_in_the_past_fails() {
  char offset_filename[PATH_MAX];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_all_past.txt", tmp_folder);
  write_freq_offset_file(offset_filename, "1 1\n2 2\n");
  int code = freq_offset_create(offset_filename, 4, 16, &fo);
  TEST_ASSERT(code != 0);
  TEST_ASSERT_NULL(fo);
}

void test_skips_past_entries_to_reach_future_one() {
  double future_ts = (double) time(NULL) + 1000000.0;
  char content[256];
  snprintf(content, sizeof(content), "1 1\n2 2\n%.6f 5\n", future_ts);
  char offset_filename[PATH_MAX];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_skip_to_future.txt", tmp_folder);
  write_freq_offset_file(offset_filename, content);

  int code = freq_offset_create(offset_filename, 4, 16, &fo);
  TEST_ASSERT_EQUAL_INT(0, code);
}

void test_missing_file_fails() {
  int code = freq_offset_create("/non-existing-directory/freq.txt", 4, 16, &fo);
  TEST_ASSERT(code != 0);
  TEST_ASSERT_NULL(fo);
}

void test_process_buffer_exceeded() {
  char offset_filename[4096];
  snprintf(offset_filename, sizeof(offset_filename), "%s/test_freq_offset_single_entry.txt", tmp_folder);
  write_freq_offset_file(offset_filename, "0 1\n");
  int code = freq_offset_create(offset_filename, 4, 4, &fo);
  TEST_ASSERT_EQUAL_INT(0, code);

  complex_input = make_dc_input(8);
  float complex *output = NULL;
  size_t output_len = 0;
  freq_offset_process(complex_input, 8, &output, &output_len, fo);
  TEST_ASSERT_NULL(output);
  TEST_ASSERT_EQUAL_INT(0, output_len);
}

void tearDown() {
  if (fo != NULL) {
    freq_offset_destroy(fo);
    fo = NULL;
  }
  if (complex_input != NULL) {
    free(complex_input);
    complex_input = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  tmp_folder = getenv("TMPDIR");
  if (tmp_folder == NULL) {
    tmp_folder = "/tmp";
  }
  UNITY_BEGIN();
  RUN_TEST(test_flat_before_first_entry_file_mode);
  RUN_TEST(test_negative_offset_rotates_the_other_way);
  RUN_TEST(test_flat_before_first_entry_realtime_future);
  RUN_TEST(test_all_entries_in_the_past_fails);
  RUN_TEST(test_skips_past_entries_to_reach_future_one);
  RUN_TEST(test_missing_file_fails);
  RUN_TEST(test_process_buffer_exceeded);
  return UNITY_END();
}
