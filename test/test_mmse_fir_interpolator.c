#include <unity.h>
#include "../src/dsp/mmse_fir_interpolator.h"
#include "utils.h"
#include <stdio.h>

mmse_fir_interpolator *interp = NULL;
float *float_input = NULL;

void test_normal() {
  int code = mmse_fir_interpolator_create(&interp);
  TEST_ASSERT_EQUAL_INT(0, code);
  setup_input_data(&float_input, 0, 8);
  float result = mmse_fir_interpolator_process(float_input, 0.14, interp);
  TEST_ASSERT(fabsl(3.140217F - result) < 0.001);
}

void tearDown() {
  if (interp != NULL) {
    mmse_fir_interpolator_destroy(interp);
    interp = NULL;
  }
  if (float_input != NULL) {
    free(float_input);
    float_input = NULL;
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
