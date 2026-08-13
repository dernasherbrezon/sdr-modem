#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/sig_source.h"
#include "utils.h"

sig_source *source = NULL;

void test_success() {
  int code = sig_source_create(1.0F, 4, 4, &source);
  TEST_ASSERT_EQUAL_INT(0, code);

  float complex *output = NULL;
  size_t output_len = 0;
  sig_source_process(1, 4, &output, &output_len, source);

  const float buffer[8] = {1, 0, 0, 1, -1, 0, 0, -1};
  assert_complex_array(buffer, sizeof(buffer) / sizeof(float) / 2, output, output_len);
}

void tearDown() {
  if (source != NULL) {
    sig_source_destroy(source);
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_success);
  return UNITY_END();
}
