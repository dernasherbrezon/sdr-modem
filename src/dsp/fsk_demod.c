#include "fsk_demod.h"
#include "lpf_complex.h"
#include "quadrature_demod.h"
#include <math.h>
#include <errno.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

struct fsk_demod_t {

	lpf_complex *lpf_ccf;
	quadrature_demod *quad_demod;
};

int create_fsk_demod(uint32_t sampling_freq, int baud_rate, float deviation, int decimation, uint32_t transition_width, bool use_dc_block, uint32_t max_input_buffer_length, fsk_demod **demod) {
	struct fsk_demod_t *result = malloc(sizeof(struct fsk_demod_t));
	if (result == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*result = (struct fsk_demod_t ) { 0 };

	float carson_cutoff = fabs(deviation) + baud_rate / 2.0f;
	int code = lpf_complex_create(sampling_freq, carson_cutoff, 0.1 * carson_cutoff, max_input_buffer_length, &result->lpf_ccf);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}
	code = quadrature_demod_create((sampling_freq / (2 * M_PI * deviation)), max_input_buffer_length, &result->quad_demod);
	if (code != 0) {
		destroy_fsk_demod(result);
		return code;
	}
	//FIXME add much more

	return 0;
}

void fsk_demodulate(const float complex *input, size_t input_len, int8_t **output, size_t *output_len, fsk_demod *demod) {
	float complex *lpf_output = NULL;
	size_t lpf_output_len = 0;
	lpf_complex_process(input, input_len, &lpf_output, &lpf_output_len, demod->lpf_ccf);

	float *qd_output = NULL;
	size_t qd_output_len = 0;
	quadrature_demod_process(lpf_output, lpf_output_len, &qd_output, &qd_output_len, demod->quad_demod);

	//FIXME go further
}

int destroy_fsk_demod(fsk_demod *demod) {
	return 0;
}
