#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/gaussian_taps.h"
#include "utils.h"

float *taps = NULL;

void test_normal() {
  size_t size = 12;
  int code = gaussian_taps_create(2 * (48000.0F / 9600), 0.5f, size, &taps);
  TEST_ASSERT_EQUAL_INT(0, code);
  const float expected[] = {0.026047f, 0.049435f, 0.081370f, 0.116161f, 0.143820f, 0.154432f, 0.143820f, 0.116161f, 0.081370f, 0.049435f, 0.026047f, 0.011903f};
  assert_float_array(expected, size, taps, size);
}

void tearDown() {
  if (taps != NULL) {
    free(taps);
    taps = NULL;
  }
}

void setUp() {
  //do nothing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_normal);
  return UNITY_END();
}
