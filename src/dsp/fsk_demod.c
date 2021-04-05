#include "fsk_demod.h"
#include "lpf_complex.h"
#include <math.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

struct fsk_demod_t {

	lpf_complex *lpf_ccf;
};

int create_fsk_demod(uint32_t sampling_freq, int baud_rate, float deviation, int decimation, uint32_t transition_width, bool use_dc_block, uint32_t max_input_buffer_length, fsk_demod **demod) {
	float carson_cutoff = fabs(deviation) + baud_rate / 2.0f;
	lpf_complex *lpf_ccf = NULL;
	int code = lpf_complex_create(sampling_freq, carson_cutoff, 0.1 * carson_cutoff, max_input_buffer_length, &lpf_ccf);
	if (code != 0) {
		return code;
	}

	//FIXME add much more

	return 0;
}

void fsk_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod) {
	float complex *cc = NULL;
	size_t cc_len = 0;
	lpf_complex_process(input, input_len, &cc, &cc_len, demod->lpf_ccf);
	//FIXME go further
}

int destroy_fsk_demod(fsk_demod *demod) {
	return 0;
}
