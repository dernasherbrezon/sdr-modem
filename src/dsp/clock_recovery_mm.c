#include "clock_recovery_mm.h"

struct clock_mm_t {

};

int clock_mm_create(float omega, float gain_omega, float mu, float gain_mu, float omega_relative_limit, size_t output_len, clock_mm **clock) {
	return 0;
}

void clock_mm_process(const float *input, size_t input_len, float **output, size_t *output_len, clock_mm *clock) {

}

void clock_mm_destroy(clock_mm *clock) {
}
