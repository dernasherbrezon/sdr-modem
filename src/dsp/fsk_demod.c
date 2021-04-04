#include "fsk_demod.h"
#include "lpf.h"
#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

struct fsk_demod_t {

	float complex **taps;
	size_t aligned_taps_len;
	size_t alignment;
	size_t taps_len;
	float *original_taps;
};

int create_fsk_demod(uint32_t sampling_freq, int baud_rate, float deviation, int decimation, uint32_t transition_width, bool use_dc_block, fsk_demod **demod) {
	float carson_cutoff = fabs(deviation) + baud_rate / 2.0f;

	float *taps = NULL;
	size_t len;
	int code = create_low_pass_filter(1.0, sampling_freq, carson_cutoff, 0.1 * carson_cutoff, &taps, &len);
	if (code != 0) {
		return code;
	}

	//FIXME add much more

	return 0;
}

void fsk_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod) {

}

int destroy_fsk_demod(fsk_demod *demod) {
	return 0;
}
