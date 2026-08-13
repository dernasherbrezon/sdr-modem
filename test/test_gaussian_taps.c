#include <stdlib.h>
#include <unity.h>
#include "../src/dsp/gaussian_taps.h"

float *taps = NULL;

void test_normal() {
  int code = gaussian_taps_create(1.5, 2 * (48000.0F / 9600), 0.5, 12, &taps);
  TEST_ASSERT_EQUAL_INT(0, code);

  const float expected_taps[] = {0.039070457f, 0.07415177f, 0.12205514f, 0.17424175f, 0.21572968f, 0.23164831f, 0.21572968f, 0.17424175f, 0.12205514f, 0.07415177f, 0.039070457f, 0.017854061f};

  for (int i = 0; i < 12; i++) {
    TEST_ASSERT_EQUAL_INT((int32_t) (taps[i] * 10000), (int32_t) (expected_taps[i] * 10000));
  }
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
